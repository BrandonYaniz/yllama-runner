#include "llama_backend.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

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

ConfigureResult configure_error(std::string code, std::string message) {
  ConfigureResult result;
  result.error = BackendError{std::move(code), std::move(message)};
  return result;
}

class LlamaBackend final : public Backend {
 public:
  LlamaBackend() {
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

  GenerateResult generate(const GenerateCommand&, const DeltaCallback&) override {
    if (!context_) {
      return GenerateResult{
          "error",
          Usage{},
          BackendError{"not_configured",
                       "Backend must be configured before generation."}};
    }

    return GenerateResult{
        "error",
        Usage{},
        BackendError{"generation_not_implemented",
                     "llama.cpp generation is not implemented yet."}};
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
