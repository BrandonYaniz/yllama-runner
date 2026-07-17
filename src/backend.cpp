#include "backend.hpp"

#include "llama_backend.hpp"

#include <cmath>

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

    if (!on_delta("fake response")) {
      GenerateResult r; r.finish_reason = FinishReason::Stop; return r;
    }
    GenerateResult r; r.finish_reason = FinishReason::Eos; r.input_tokens = 1;
    r.output_tokens = 2; return r;
  }

 private:
  bool configured_ = false;
  std::string model_path_;
  int context_tokens_ = 0;
  int threads_ = 0;
};

}  // namespace

TokenizationFlags tokenization_flags(TokenizationMode mode) {
  return mode == TokenizationMode::Raw ? TokenizationFlags{true, false}
                                       : TokenizationFlags{false, true};
}

std::optional<BackendError> validate_generate_request(const GenerateRequest& r) {
  const auto& o = r.options;
  auto finite = [](double v) { return std::isfinite(v); };
  if (r.prompt.size() > 16U * 1024U * 1024U)
    return BackendError{"prompt_too_large", "prompt exceeds 16 MiB"};
  if (o.max_tokens == 0 || o.max_tokens > 1000000)
    return BackendError{"invalid_max_tokens", "max_tokens must be in [1, 1000000]"};
  if (!finite(o.temperature) || o.temperature < 0 || o.temperature > 100)
    return BackendError{"invalid_temperature", "temperature must be finite and in [0, 100]"};
  if (!finite(o.top_p) || o.top_p <= 0 || o.top_p > 1)
    return BackendError{"invalid_top_p", "top_p must be finite and in (0, 1]"};
  if (o.top_k < 0 || o.top_k > 1000000)
    return BackendError{"invalid_top_k", "top_k must be in [0, 1000000]"};
  if (!finite(o.min_p) || o.min_p < 0 || o.min_p > 1)
    return BackendError{"invalid_min_p", "min_p must be finite and in [0, 1]"};
  if (!finite(o.presence_penalty) || o.presence_penalty < -2 || o.presence_penalty > 2)
    return BackendError{"invalid_presence_penalty", "presence_penalty must be finite and in [-2, 2]"};
  if (!finite(o.repeat_penalty) || o.repeat_penalty <= 0 || o.repeat_penalty > 100)
    return BackendError{"invalid_repeat_penalty", "repeat_penalty must be finite and in (0, 100]"};
  return std::nullopt;
}

std::unique_ptr<Backend> make_fake_backend() {
  return std::make_unique<FakeBackend>();
}

std::unique_ptr<Backend> make_default_backend() {
  return make_llama_backend();
}

}  // namespace yllama
