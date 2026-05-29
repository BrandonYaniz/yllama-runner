#ifndef YLLAMA_RUNNER_PROMPT_HPP
#define YLLAMA_RUNNER_PROMPT_HPP

#include <string>

#include "jsonl.hpp"

namespace yllama {

std::string render_prompt(const GenerateInput& input);

}  // namespace yllama

#endif
