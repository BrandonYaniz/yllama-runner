#include "prompt.hpp"

#include <cassert>
#include <string>

int main() {
  {
    const yllama::GenerateInput input = yllama::PromptInput{"Explain this error."};
    assert(yllama::render_prompt(input) == "Explain this error.");
  }

  {
    yllama::MessagesInput messages;
    messages.messages.push_back({"system", "Answer clearly."});
    messages.messages.push_back({"user", "Explain this error."});
    messages.messages.push_back({"assistant", "It means the file is missing."});
    messages.messages.push_back({"user", "What should I check next?"});

    const yllama::GenerateInput input = messages;
    assert(yllama::render_prompt(input) ==
           "system: Answer clearly.\n\n"
           "user: Explain this error.\n\n"
           "assistant: It means the file is missing.\n\n"
           "user: What should I check next?\n\n"
           "assistant:");
  }

  {
    const yllama::GenerateInput input = yllama::MessagesInput{};
    assert(yllama::render_prompt(input) == "assistant:");
  }

  return 0;
}
