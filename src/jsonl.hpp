#ifndef YLLAMA_RUNNER_JSONL_HPP
#define YLLAMA_RUNNER_JSONL_HPP

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace yllama {

struct ParseError {
  std::string code;
  std::string message;
};

struct ConfigureCommand {
  std::string id;
  std::string model_path;
  int context_tokens = 0;
  int threads = 0;
};

struct PromptInput {
  std::string prompt;
};

struct Message {
  std::string role;
  std::string content;
};

struct MessagesInput {
  std::vector<Message> messages;
};

using GenerateInput = std::variant<PromptInput, MessagesInput>;

struct GenerateSettings {
  std::optional<double> temperature;
  std::optional<double> top_p;
  std::optional<int> max_tokens;
  std::optional<bool> stream;
  std::string output_format = "json";
  std::string output_delivery = "stream";
  std::vector<std::string> stop;
};

struct GenerateCommand {
  std::string id;
  GenerateInput input;
  GenerateSettings settings;
};

struct CancelCommand {
  std::string id;
};

struct ShutdownCommand {
  std::string id;
};

using Command =
    std::variant<ConfigureCommand, GenerateCommand, CancelCommand, ShutdownCommand>;

struct ParseResult {
  std::optional<Command> command;
  std::optional<ParseError> error;
};

ParseResult parse_command_line(std::string_view line);

}  // namespace yllama

#endif
