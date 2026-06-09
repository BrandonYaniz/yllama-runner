#include "llama_backend.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <llama.h>

#include "prompt.hpp"

namespace yllama {
namespace {

constexpr int kDefaultMaxTokens = 128;
constexpr double kDefaultTemperature = 0.8;
constexpr double kDefaultTopP = 0.95;

class LlamaRuntime {
 public:
  LlamaRuntime() {
    llama_backend_init();
  }

  ~LlamaRuntime() {
    llama_backend_free();
  }

  LlamaRuntime(const LlamaRuntime&) = delete;
  LlamaRuntime& operator=(const LlamaRuntime&) = delete;
};

LlamaRuntime& runtime() {
  static LlamaRuntime instance;
  return instance;
}

#ifndef YLLAMA_ENABLE_LLAMA_LOGS
void quiet_llama_log(ggml_log_level, const char*, void*) {}
#endif

struct LlamaModelDeleter {
  void operator()(llama_model* model) const {
    if (model != nullptr) {
      llama_model_free(model);
    }
  }
};

struct LlamaContextDeleter {
  void operator()(llama_context* context) const {
    if (context != nullptr) {
      llama_free(context);
    }
  }
};

using ModelPtr = std::unique_ptr<llama_model, LlamaModelDeleter>;
using ContextPtr = std::unique_ptr<llama_context, LlamaContextDeleter>;

struct LlamaSamplerDeleter {
  void operator()(llama_sampler* sampler) const {
    if (sampler != nullptr) {
      llama_sampler_free(sampler);
    }
  }
};

using SamplerPtr = std::unique_ptr<llama_sampler, LlamaSamplerDeleter>;

ConfigureResult configure_error(std::string code, std::string message) {
  ConfigureResult result;
  result.error = BackendError{std::move(code), std::move(message)};
  return result;
}

GenerateResult generate_error(std::string code, std::string message) {
  GenerateResult result;
  result.finish_reason = "error";
  result.error = BackendError{std::move(code), std::move(message)};
  return result;
}

std::optional<std::vector<llama_token>> tokenize_prompt(
    const llama_vocab* vocab,
    std::string_view prompt) {
  const int count = -llama_tokenize(vocab, prompt.data(), prompt.size(), nullptr,
                                    0, true, true);
  if (count <= 0) {
    return std::nullopt;
  }

  std::vector<llama_token> tokens(static_cast<std::size_t>(count));
  const int written = llama_tokenize(vocab, prompt.data(), prompt.size(),
                                     tokens.data(), tokens.size(), true, true);
  if (written < 0) {
    return std::nullopt;
  }

  tokens.resize(static_cast<std::size_t>(written));
  return tokens;
}

std::optional<std::string> token_to_text(const llama_vocab* vocab,
                                         llama_token token) {
  std::vector<char> buffer(128);
  int written =
      llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, true);
  if (written < 0) {
    buffer.resize(static_cast<std::size_t>(-written));
    written =
        llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, true);
  }
  if (written < 0) {
    return std::nullopt;
  }
  return std::string(buffer.data(), static_cast<std::size_t>(written));
}

SamplerPtr make_sampler(const GenerateSettings& settings) {
  SamplerPtr sampler(llama_sampler_chain_init(llama_sampler_chain_default_params()));
  if (!sampler) {
    return nullptr;
  }

  const double temperature = settings.temperature.value_or(kDefaultTemperature);
  if (temperature <= 0.0) {
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_greedy());
    return sampler;
  }

  const double top_p = settings.top_p.value_or(kDefaultTopP);
  llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(
                                             static_cast<float>(top_p), 1));
  llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(
                                             static_cast<float>(temperature)));
  llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
  return sampler;
}

std::optional<std::size_t> first_stop_position(
    std::string_view text,
    const std::vector<std::string>& stops) {
  std::optional<std::size_t> earliest;
  for (const std::string& stop : stops) {
    if (stop.empty()) {
      continue;
    }
    const std::size_t found = text.find(std::string_view(stop));
    if (found != std::string_view::npos && (!earliest || found < *earliest)) {
      earliest = found;
    }
  }
  return earliest;
}

std::size_t max_stop_size(const std::vector<std::string>& stops) {
  std::size_t result = 0;
  for (const std::string& stop : stops) {
    result = std::max(result, stop.size());
  }
  return result;
}

class LlamaBackend final : public Backend {
 public:
  LlamaBackend() {
#ifndef YLLAMA_ENABLE_LLAMA_LOGS
    llama_log_set(quiet_llama_log, nullptr);
#endif
    static_cast<void>(runtime());
  }

  ConfigureResult configure(const ConfigureCommand& command) override {
    if (command.context_tokens <= 0) {
      return configure_error("invalid_config",
                             "context_tokens must be greater than zero.");
    }
    if (command.threads <= 0) {
      return configure_error("invalid_config", "threads must be greater than zero.");
    }

    ContextPtr next_context;
    ModelPtr next_model;

    llama_model_params model_params = llama_model_default_params();
    next_model.reset(
        llama_model_load_from_file(command.model_path.c_str(), model_params));
    if (!next_model) {
      return configure_error("model_load_failed", "Unable to load model.");
    }

    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = static_cast<std::uint32_t>(command.context_tokens);
    context_params.n_threads = command.threads;
    context_params.n_threads_batch = command.threads;

    next_context.reset(llama_init_from_model(next_model.get(), context_params));
    if (!next_context) {
      return configure_error("context_init_failed",
                             "Unable to initialize model context.");
    }

    model_ = std::move(next_model);
    context_ = std::move(next_context);
    model_path_ = command.model_path;
    context_tokens_ = static_cast<int>(llama_n_ctx(context_.get()));
    return ConfigureResult{std::nullopt, model_path_, context_tokens_};
  }

  GenerateResult generate(const GenerateCommand& command,
                          const DeltaCallback& on_delta,
                          const CancellationCallback& is_cancelled) override {
    if (!context_) {
      return generate_error("not_configured",
                            "Backend must be configured before generation.");
    }

    const int max_tokens = command.settings.max_tokens.value_or(kDefaultMaxTokens);
    if (max_tokens <= 0) {
      return generate_error("invalid_settings",
                            "max_tokens must be greater than zero.");
    }
    if (command.settings.temperature && *command.settings.temperature < 0.0) {
      return generate_error("invalid_settings",
                            "temperature must be greater than or equal to zero.");
    }
    if (command.settings.top_p &&
        (*command.settings.top_p <= 0.0 || *command.settings.top_p > 1.0)) {
      return generate_error("invalid_settings", "top_p must be in the range (0, 1].");
    }

    const llama_vocab* vocab = llama_model_get_vocab(model_.get());
    const std::string prompt = render_prompt(command.input);
    if (is_cancelled()) {
      return GenerateResult{"cancelled", Usage{}, std::nullopt};
    }

    auto prompt_tokens = tokenize_prompt(vocab, prompt);
    if (!prompt_tokens) {
      return generate_error("tokenize_failed", "Unable to tokenize prompt.");
    }

    if (static_cast<int>(prompt_tokens->size()) >= context_tokens_) {
      return generate_error("context_exceeded",
                            "Prompt does not fit in the configured context.");
    }

    SamplerPtr sampler = make_sampler(command.settings);
    if (!sampler) {
      return generate_error("sampler_init_failed",
                            "Unable to initialize sampler.");
    }

    llama_memory_clear(llama_get_memory(context_.get()), true);

    llama_batch batch =
        llama_batch_get_one(prompt_tokens->data(), prompt_tokens->size());
    int n_pos = 0;
    int output_tokens = 0;
    std::string generated;
    std::size_t emitted = 0;
    const std::size_t stop_holdback = max_stop_size(command.settings.stop);

    auto emit_available = [&](bool final) {
      if (emitted >= generated.size()) {
        return;
      }

      std::size_t emit_until = generated.size();
      if (!final && stop_holdback > 1 && emit_until - emitted >= stop_holdback) {
        emit_until -= stop_holdback - 1;
      } else if (!final && stop_holdback > 1) {
        return;
      }

      if (emit_until > emitted) {
        on_delta(std::string_view(generated).substr(emitted, emit_until - emitted));
        emitted = emit_until;
      }
    };

    std::string finish_reason = "length";
    while (n_pos + batch.n_tokens < context_tokens_ &&
           output_tokens < max_tokens) {
      if (is_cancelled()) {
        finish_reason = "cancelled";
        break;
      }

      if (llama_decode(context_.get(), batch) != 0) {
        return generate_error("decode_failed", "llama.cpp failed to decode tokens.");
      }
      n_pos += batch.n_tokens;

      if (is_cancelled()) {
        finish_reason = "cancelled";
        break;
      }

      llama_token next_token = llama_sampler_sample(sampler.get(), context_.get(), -1);
      if (llama_vocab_is_eog(vocab, next_token)) {
        finish_reason = "stop";
        break;
      }

      auto piece = token_to_text(vocab, next_token);
      if (!piece) {
        return generate_error("detokenize_failed",
                              "Unable to convert token to text.");
      }

      ++output_tokens;
      generated += *piece;

      if (const auto stop_at =
              first_stop_position(generated, command.settings.stop)) {
        if (*stop_at > emitted) {
          on_delta(std::string_view(generated).substr(emitted, *stop_at - emitted));
          emitted = *stop_at;
        }
        generated.resize(*stop_at);
        finish_reason = "stop";
        break;
      }

      emit_available(false);
      batch = llama_batch_get_one(&next_token, 1);
    }

    if (finish_reason != "stop" && output_tokens < max_tokens) {
      finish_reason = "length";
    }
    emit_available(true);

    return GenerateResult{finish_reason,
                          Usage{static_cast<int>(prompt_tokens->size()),
                                output_tokens},
                          std::nullopt};
  }

 private:
  ModelPtr model_;
  ContextPtr context_;
  std::string model_path_;
  int context_tokens_ = 0;
};

}  // namespace

std::unique_ptr<Backend> make_llama_backend() {
  return std::make_unique<LlamaBackend>();
}

}  // namespace yllama
