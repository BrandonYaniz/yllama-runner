#include "backend.hpp"

namespace yllama {
namespace {

class FakeBackend final : public Backend {
 public:
  ConfigureResult configure(const ConfigureCommand& command) override {
    model_path_ = command.model_path;
    context_tokens_ = command.context_tokens;
    threads_ = command.threads;
    configured_ = true;
    return {};
  }

  GenerateResult generate(const GenerateCommand&,
                          const DeltaCallback& on_delta) override {
    if (!configured_) {
      return GenerateResult{"error",
                            Usage{},
                            BackendError{
                                "not_configured",
                                "Backend must be configured before generation."}};
    }

    on_delta("fake response");
    return GenerateResult{"stop", Usage{0, 2}, std::nullopt};
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

}  // namespace yllama
