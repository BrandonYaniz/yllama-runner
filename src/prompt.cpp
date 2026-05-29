#include "prompt.hpp"

#include <sstream>

namespace yllama {
namespace {

std::string render_messages(const MessagesInput& input) {
  std::ostringstream out;

  for (const Message& message : input.messages) {
    out << message.role << ": " << message.content << "\n\n";
  }

  out << "assistant:";
  return out.str();
}

}  // namespace

std::string render_prompt(const GenerateInput& input) {
  if (const auto* prompt = std::get_if<PromptInput>(&input)) {
    return prompt->prompt;
  }

  return render_messages(std::get<MessagesInput>(input));
}

}  // namespace yllama
