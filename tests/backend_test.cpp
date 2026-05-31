#include "backend.hpp"

#include <cassert>
#include <string>

int main() {
  auto backend = yllama::make_fake_backend();

  yllama::GenerateCommand command;
  command.id = "req-001";
  command.input = yllama::PromptInput{"Hello"};

  {
    const auto result = backend->generate(
        command, [](std::string_view) {}, []() { return false; });
    assert(result.error.has_value());
    assert(result.error->code == "not_configured");
  }

  {
    const auto result = backend->configure(
        yllama::ConfigureCommand{"cfg-001", "/models/fast/model.gguf", 8192, 4});
    assert(!result.error.has_value());
    assert(result.model_path == "/models/fast/model.gguf");
    assert(result.context_tokens == 8192);
  }

  {
    std::string delta;
    const auto result = backend->generate(
        command, [&](std::string_view text) { delta.append(text); },
        []() { return false; });
    assert(!result.error.has_value());
    assert(delta == "fake response");
    assert(result.finish_reason == "stop");
    assert(result.usage.input_tokens == 1);
    assert(result.usage.output_tokens == 2);
  }

  {
    std::string delta;
    const auto result = backend->generate(
        command, [&](std::string_view text) { delta.append(text); },
        []() { return true; });
    assert(!result.error.has_value());
    assert(delta.empty());
    assert(result.finish_reason == "cancelled");
    assert(result.usage.input_tokens == 1);
    assert(result.usage.output_tokens == 0);
  }

  return 0;
}
