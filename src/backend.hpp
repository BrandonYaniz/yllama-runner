#ifndef YLLAMA_RUNNER_BACKEND_HPP
#define YLLAMA_RUNNER_BACKEND_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace yllama {

struct BackendError {
  std::string code;
  std::string message;
};

struct RunnerConfig {
  std::string model_path;
  int context_tokens = 0;
  int threads = 0;
};

struct ConfigureResult {
  std::optional<BackendError> error;
  std::string model_path;
  int context_tokens = 0;
};

struct GenerateOptions {
  double temperature = 0.8;
  double top_p = 0.95;
  int max_tokens = 128;
};

struct GenerateResult {
  std::optional<BackendError> error;
};

using DeltaCallback = std::function<void(std::string_view text)>;
using CancellationCallback = std::function<bool()>;

class Backend {
 public:
  virtual ~Backend() = default;

  virtual ConfigureResult configure(const RunnerConfig& config) = 0;
  virtual GenerateResult generate(std::string_view prompt,
                                  const GenerateOptions& options,
                                  const DeltaCallback& on_delta,
                                  const CancellationCallback& is_cancelled) = 0;
};

std::unique_ptr<Backend> make_fake_backend();
std::unique_ptr<Backend> make_default_backend();

}  // namespace yllama

#endif
