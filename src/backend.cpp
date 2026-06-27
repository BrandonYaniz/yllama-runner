#include "backend.hpp"

#include "llama_backend.hpp"

namespace yllama {
namespace {

class FakeBackend final : public Backend {
 public:
  ConfigureResult configure(const RunnerConfig& config) override {
    model_path_ = config.model_path;
    context_tokens_ = config.context_tokens;
    threads_ = config.threads;
    configured_ = true;
    return ConfigureResult{std::nullopt, model_path_, context_tokens_};
  }

  GenerateResult generate(std::string_view,
                          const GenerateOptions&,
                          const DeltaCallback& on_delta,
                          const CancellationCallback& is_cancelled) override {
    if (!configured_) {
      return GenerateResult{BackendError{
          "not_configured", "Backend must be configured before generation."}};
    }

    if (is_cancelled()) {
      return GenerateResult{std::nullopt};
    }

    on_delta("fake response");
    return GenerateResult{std::nullopt};
  }

 private:
  bool configured_ = false;
  std::string model_path_;
  int context_tokens_ = 0;
  int threads_ = 0;
};

}  // namespace

std::unique_ptr<Backend> make_fake_backend() {
  return std::make_unique<FakeBackend>();
}

std::unique_ptr<Backend> make_default_backend() {
  return make_llama_backend();
}

}  // namespace yllama
