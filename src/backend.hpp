#ifndef YLLAMA_RUNNER_BACKEND_HPP
#define YLLAMA_RUNNER_BACKEND_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yllama {

struct BackendError { std::string code; std::string message; };

struct RunnerConfig {
  std::string model_path;
  int context_tokens = 0;
  int threads = 0;
  int gpu_layers = 0;
  std::uint16_t protocol = 1;
  std::string runner_version;
};

struct ConfigureResult {
  std::optional<BackendError> error;
  std::string model_path;
  int context_tokens = 0;
};

enum class TokenizationMode : std::uint8_t { Raw = 0, Preformatted = 1 };
struct TokenizationFlags { bool add_special; bool parse_special; };
TokenizationFlags tokenization_flags(TokenizationMode mode);
enum class FinishReason : std::uint8_t { Eos = 0, Length = 1, Stop = 2, Cancelled = 3 };

struct GenerateOptions {
  std::uint32_t max_tokens = 128;
  double temperature = 0.8;
  double top_p = 0.95;
  std::int32_t top_k = 40;
  double min_p = 0.05;
  double presence_penalty = 0.0;
  double repeat_penalty = 1.0;
  std::uint64_t seed = UINT64_MAX;
  TokenizationMode tokenization_mode = TokenizationMode::Raw;
  std::vector<std::string> stop_sequences;
};

struct GenerateRequest { std::string prompt; GenerateOptions options; };

struct GenerateResult {
  std::optional<BackendError> error;
  FinishReason finish_reason = FinishReason::Eos;
  std::uint32_t input_tokens = 0;
  std::uint32_t output_tokens = 0;
  std::uint64_t prompt_microseconds = 0;
  std::uint64_t generation_microseconds = 0;
};

using DeltaCallback = std::function<bool(std::string_view text)>;
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

std::optional<BackendError> validate_generate_request(const GenerateRequest& request);
std::unique_ptr<Backend> make_fake_backend();
std::unique_ptr<Backend> make_default_backend();
}  // namespace yllama
#endif
