#include "backend.hpp"

#include <cctype>

#include "llama_backend.hpp"

#include "prompt.hpp"

namespace yllama {
namespace {

int count_input_tokens(std::string_view prompt) {
  int tokens = 0;
  bool in_token = false;

  for (const unsigned char ch : prompt) {
    if (std::isspace(ch)) {
      in_token = false;
      continue;
    }
    if (!in_token) {
      ++tokens;
      in_token = true;
    }
  }

  return tokens;
}

class FakeBackend final : public Backend {
 public:
  ConfigureResult configure(const ConfigureCommand& command) override {
    model_path_ = command.model_path;
    context_tokens_ = command.context_tokens;
    threads_ = command.threads;
    configured_ = true;
    return ConfigureResult{std::nullopt, model_path_, context_tokens_};
  }

  GenerateResult generate(const GenerateCommand& command,
                          const DeltaCallback& on_delta,
                          const CancellationCallback& is_cancelled) override {
    if (!configured_) {
      return GenerateResult{"error",
                            Usage{},
                            BackendError{
                                "not_configured",
                                "Backend must be configured before generation."}};
    }

    const std::string prompt = render_prompt(command.input);
    if (is_cancelled()) {
      return GenerateResult{"cancelled", Usage{count_input_tokens(prompt), 0},
                            std::nullopt};
    }

    on_delta("fake response");
    return GenerateResult{"stop",
                          Usage{count_input_tokens(prompt), 2},
                          std::nullopt};
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
