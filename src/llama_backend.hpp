#ifndef YLLAMA_RUNNER_LLAMA_BACKEND_HPP
#define YLLAMA_RUNNER_LLAMA_BACKEND_HPP

#include <memory>

#include "backend.hpp"

namespace yllama {

std::unique_ptr<Backend> make_llama_backend();

}  // namespace yllama

#endif
