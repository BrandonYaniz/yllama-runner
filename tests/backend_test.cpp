#include "backend.hpp"

#include <cassert>
#include <string>

int main() {
  auto backend = yllama::make_fake_backend();

  yllama::GenerateCommand command;
  command.id = "req-001";
  command.input = yllama::PromptInput{"Hello"};

  {
    const auto result = backend->generate(command, [](std::string_view) {});
    assert(result.error.has_value());
    assert(result.error->code == "not_configured");
  }

  {
    const auto result = backend->configure(
        yllama::ConfigureCommand{"cfg-001", "/models/fast/model.gguf", 8192, 4});
    assert(!result.error.has_value());
  }

  {
    std::string delta;
    const auto result = backend->generate(
        command, [&](std::string_view text) { delta.append(text); });
    assert(!result.error.has_value());
    assert(delta == "fake response");
    assert(result.finish_reason == "stop");
    assert(result.usage.input_tokens == 0);
    assert(result.usage.output_tokens == 2);
  }

  return 0;
}
