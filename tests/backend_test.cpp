#include "backend.hpp"

#include <cassert>
#include <string>

int main() {
  auto backend = yllama::make_fake_backend();

  yllama::GenerateOptions options;

  {
    const auto result = backend->generate(
        "Hello", options, [](std::string_view) { return true; }, []() { return false; });
    assert(result.error.has_value());
    assert(result.error->code == "not_configured");
  }

  {
    const auto result =
        backend->configure(yllama::RunnerConfig{"/models/fast/model.gguf", 8192, 4});
    assert(!result.error.has_value());
    assert(result.model_path == "/models/fast/model.gguf");
    assert(result.context_tokens == 8192);
  }

  {
    std::string delta;
    const auto result = backend->generate(
        "Hello", options, [&](std::string_view text) { delta.append(text); return true; },
        []() { return false; });
    assert(!result.error.has_value());
    assert(delta == "fake response");
  }

  {
    std::string delta;
    const auto result = backend->generate(
        "Hello", options, [&](std::string_view text) { delta.append(text); return true; },
        []() { return true; });
    assert(!result.error.has_value());
    assert(delta.empty());
  }

  return 0;
}
