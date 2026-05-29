#ifndef YLLAMA_RUNNER_BACKEND_HPP
#define YLLAMA_RUNNER_BACKEND_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "jsonl.hpp"
#include "protocol.hpp"

namespace yllama {

struct BackendError {
  std::string code;
  std::string message;
};

struct ConfigureResult {
  std::optional<BackendError> error;
};

struct GenerateResult {
  std::string finish_reason = "stop";
  Usage usage;
  std::optional<BackendError> error;
};

using DeltaCallback = std::function<void(std::string_view text)>;

class Backend {
 public:
  virtual ~Backend() = default;

  virtual ConfigureResult configure(const ConfigureCommand& command) = 0;
  virtual GenerateResult generate(const GenerateCommand& command,
                                  const DeltaCallback& on_delta) = 0;
};

std::unique_ptr<Backend> make_fake_backend();

}  // namespace yllama

#endif
