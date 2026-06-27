#include "llama_backend.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <llama.h>

namespace yllama {
namespace {

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

SamplerPtr make_sampler(const GenerateOptions& options) {
  SamplerPtr sampler(llama_sampler_chain_init(llama_sampler_chain_default_params()));
  if (!sampler) {
    return nullptr;
  }

  if (options.temperature <= 0.0) {
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_greedy());
    return sampler;
  }

  llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(
                                             static_cast<float>(options.top_p), 1));
  llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(
                                             static_cast<float>(options.temperature)));
  llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
  return sampler;
}

class LlamaBackend final : public Backend {
 public:
  LlamaBackend() {
#ifndef YLLAMA_ENABLE_LLAMA_LOGS
    llama_log_set(quiet_llama_log, nullptr);
#endif
    static_cast<void>(runtime());
  }

  ConfigureResult configure(const RunnerConfig& config) override {
    if (config.context_tokens <= 0) {
      return configure_error("invalid_config",
                             "context_tokens must be greater than zero.");
    }
    if (config.threads <= 0) {
      return configure_error("invalid_config", "threads must be greater than zero.");
    }

    ContextPtr next_context;
    ModelPtr next_model;

    llama_model_params model_params = llama_model_default_params();
    next_model.reset(
        llama_model_load_from_file(config.model_path.c_str(), model_params));
    if (!next_model) {
      return configure_error("model_load_failed", "Unable to load model.");
    }

    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = static_cast<std::uint32_t>(config.context_tokens);
    context_params.n_threads = config.threads;
    context_params.n_threads_batch = config.threads;

    next_context.reset(llama_init_from_model(next_model.get(), context_params));
    if (!next_context) {
      return configure_error("context_init_failed",
                             "Unable to initialize model context.");
    }

    model_ = std::move(next_model);
    context_ = std::move(next_context);
    model_path_ = config.model_path;
    context_tokens_ = static_cast<int>(llama_n_ctx(context_.get()));
    return ConfigureResult{std::nullopt, model_path_, context_tokens_};
  }

  GenerateResult generate(std::string_view prompt,
                          const GenerateOptions& options,
                          const DeltaCallback& on_delta,
                          const CancellationCallback& is_cancelled) override {
    if (!context_) {
      return generate_error("not_configured",
                            "Backend must be configured before generation.");
    }

    if (options.max_tokens <= 0) {
      return generate_error("invalid_settings",
                            "max_tokens must be greater than zero.");
    }
    if (options.temperature < 0.0) {
      return generate_error("invalid_settings",
                            "temperature must be greater than or equal to zero.");
    }
    if (options.top_p <= 0.0 || options.top_p > 1.0) {
      return generate_error("invalid_settings", "top_p must be in the range (0, 1].");
    }

    const llama_vocab* vocab = llama_model_get_vocab(model_.get());
    if (is_cancelled()) {
      return GenerateResult{std::nullopt};
    }

    auto prompt_tokens = tokenize_prompt(vocab, prompt);
    if (!prompt_tokens) {
      return generate_error("tokenize_failed", "Unable to tokenize prompt.");
    }

    if (static_cast<int>(prompt_tokens->size()) >= context_tokens_) {
      return generate_error("context_exceeded",
                            "Prompt does not fit in the configured context.");
    }

    SamplerPtr sampler = make_sampler(options);
    if (!sampler) {
      return generate_error("sampler_init_failed",
                            "Unable to initialize sampler.");
    }

    llama_memory_clear(llama_get_memory(context_.get()), true);

    llama_batch batch =
        llama_batch_get_one(prompt_tokens->data(), prompt_tokens->size());
    int n_pos = 0;
    int output_tokens = 0;

    while (n_pos + batch.n_tokens < context_tokens_ &&
           output_tokens < options.max_tokens) {
      if (is_cancelled()) {
        break;
      }

      if (llama_decode(context_.get(), batch) != 0) {
        return generate_error("decode_failed", "llama.cpp failed to decode tokens.");
      }
      n_pos += batch.n_tokens;

      if (is_cancelled()) {
        break;
      }

      llama_token next_token = llama_sampler_sample(sampler.get(), context_.get(), -1);
      if (llama_vocab_is_eog(vocab, next_token)) {
        break;
      }

      auto piece = token_to_text(vocab, next_token);
      if (!piece) {
        return generate_error("detokenize_failed",
                              "Unable to convert token to text.");
      }

      ++output_tokens;
      on_delta(*piece);
      batch = llama_batch_get_one(&next_token, 1);
    }

    return GenerateResult{std::nullopt};
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
