#include "jsonl.hpp"

#include <cassert>
#include <string>

namespace {

template <typename T>
const T& get_command(const yllama::ParseResult& result) {
  assert(result.command.has_value());
  assert(!result.error.has_value());
  return std::get<T>(*result.command);
}

void expect_error(const yllama::ParseResult& result, const std::string& code) {
  assert(!result.command.has_value());
  assert(result.error.has_value());
  assert(result.error->code == code);
}

}  // namespace

int main() {
  {
    const auto result = yllama::parse_command_line(
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}");
    const auto& command = get_command<yllama::ConfigureCommand>(result);
    assert(command.id == "cfg-001");
    assert(command.model_path == "/models/fast/model.gguf");
    assert(command.context_tokens == 8192);
    assert(command.threads == 4);
  }

  {
    const auto result = yllama::parse_command_line(
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\\nthere\"},"
        "\"settings\":{\"temperature\":0.2,\"top_p\":0.9,"
        "\"max_tokens\":128,\"stop\":[\"</s>\"],\"debug\":false}}");
    const auto& command = get_command<yllama::GenerateCommand>(result);
    assert(command.id == "req-001");
    const auto& input = std::get<yllama::PromptInput>(command.input);
    assert(input.prompt == "Hello\nthere");
    assert(command.settings.temperature == 0.2);
    assert(command.settings.top_p == 0.9);
    assert(command.settings.max_tokens == 128);
    assert(command.settings.stop.size() == 1);
    assert(command.settings.stop[0] == "</s>");
  }

  {
    const auto result = yllama::parse_command_line(
        "{\"type\":\"generate\",\"id\":\"req-002\","
        "\"input\":{\"kind\":\"messages\",\"messages\":["
        "{\"role\":\"system\",\"content\":\"Answer clearly.\"},"
        "{\"role\":\"user\",\"content\":\"Explain this.\"}]},"
        "\"settings\":{\"max_tokens\":64}}");
    const auto& command = get_command<yllama::GenerateCommand>(result);
    const auto& input = std::get<yllama::MessagesInput>(command.input);
    assert(input.messages.size() == 2);
    assert(input.messages[0].role == "system");
    assert(input.messages[1].content == "Explain this.");
    assert(command.settings.max_tokens == 64);
  }

  {
    const auto result =
        yllama::parse_command_line("{\"type\":\"cancel\",\"id\":\"req-001\"}");
    const auto& command = get_command<yllama::CancelCommand>(result);
    assert(command.id == "req-001");
  }

  {
    const auto result = yllama::parse_command_line(
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}");
    const auto& command = get_command<yllama::ShutdownCommand>(result);
    assert(command.id == "shutdown-001");
  }

  expect_error(yllama::parse_command_line("{"), "invalid_json");
  expect_error(yllama::parse_command_line("{\"type\":\"unknown\",\"id\":\"x\"}"),
               "unknown_command");
  expect_error(yllama::parse_command_line("{\"type\":\"shutdown\"}"),
               "invalid_command");
  expect_error(yllama::parse_command_line(
                   "{\"type\":\"configure\",\"id\":\"cfg-001\","
                   "\"context_tokens\":8192,\"threads\":4}"),
               "invalid_command");
  expect_error(yllama::parse_command_line(
                   "{\"type\":\"generate\",\"id\":\"req-003\","
                   "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
                   "\"settings\":{\"max_tokens\":4.5}}"),
               "invalid_command");

  return 0;
}
