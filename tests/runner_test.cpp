#include "runner.hpp"

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>

#include "backend.hpp"
#include "frame.hpp"

namespace {
class EchoBackend final : public yllama::Backend {
 public:
  std::optional<yllama::BackendError> configure(const yllama::RunnerConfig&) override {
    ++configure_count;
    return configure_error;
  }

  yllama::GenerateResult generate(
      std::string_view prompt, const yllama::GenerateOptions&,
      const yllama::DeltaCallback& on_delta,
      const yllama::CancellationCallback& is_cancelled) override {
    ++generate_count;
    if (prompt == "fail") {
      return yllama::GenerateResult{yllama::BackendError{"invalid_prompt", "prompt rejected"}};
    }
    if (prompt == "fatal") {
      yllama::GenerateResult result;
      result.error = yllama::BackendError{"fatal_backend", "fatal backend error",
                                          yllama::ErrorDisposition::Fatal};
      return result;
    }
    yllama::GenerateResult result;
    if (is_cancelled() || !on_delta("echo:")) {
      result.finish_reason = yllama::FinishReason::Cancelled;
      return result;
    }
    on_delta(prompt);
    result.input_tokens = 1;
    result.output_tokens = 2;
    return result;
  }

  std::optional<yllama::BackendError> configure_error;
  int configure_count = 0;
  int generate_count = 0;
};

std::string u32(unsigned int value) {
  std::string out;
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>(value >> (8 * i)));
  return out;
}
std::string u64(std::uint64_t value) {
  std::string out;
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>(value >> (8 * i)));
  return out;
}
std::string f64(double value) {
  std::uint64_t bits;
  std::memcpy(&bits, &value, sizeof(value));
  return u64(bits);
}
std::string envelope(unsigned char type, const std::string& payload = {}) {
  return std::string(1, static_cast<char>(type)) + u32(payload.size()) + payload;
}
std::string generate(const std::string& prompt) {
  std::string payload("\x00\x00", 2);
  payload += u32(8) + f64(0) + f64(1) + u32(0) + f64(0) + f64(0) +
             f64(1) + u64(1) + prompt;
  return envelope(yllama::kMessageGenerate, payload);
}

std::size_t count_frames(const std::string& bytes, unsigned char type) {
  std::size_t count = 0;
  for (std::size_t offset = 0; offset + 5 <= bytes.size();) {
    const auto frame_type = static_cast<unsigned char>(bytes[offset]);
    const auto size = static_cast<unsigned char>(bytes[offset + 1]) |
        (static_cast<unsigned char>(bytes[offset + 2]) << 8) |
        (static_cast<unsigned char>(bytes[offset + 3]) << 16) |
        (static_cast<unsigned char>(bytes[offset + 4]) << 24);
    assert(offset + 5 + size <= bytes.size());
    if (frame_type == type) ++count;
    offset += 5 + size;
  }
  return count;
}
}  // namespace

int main() {
  yllama::RunnerConfig config{"/models/model.gguf", 8192, 4, 0};

  {
    EchoBackend backend;
    std::istringstream in(generate("one") + generate("two"));
    std::ostringstream out, err;
    assert(yllama::run_stdio(in, out, err, config, backend) == 0);
    assert(backend.configure_count == 1 && backend.generate_count == 2);
    assert(count_frames(out.str(), yllama::kFrameReady) == 1);
    assert(count_frames(out.str(), yllama::kFrameCompleted) == 2);
  }
  {
    EchoBackend backend;
    std::istringstream in(generate("fail") + generate("after"));
    std::ostringstream out, err;
    assert(yllama::run_stdio(in, out, err, config, backend) == 0);
    assert(backend.generate_count == 2);
    assert(count_frames(out.str(), yllama::kFrameError) == 1);
  }
  {
    EchoBackend backend;
    std::istringstream in(generate("fatal") + generate("after"));
    std::ostringstream out, err;
    assert(yllama::run_stdio(in, out, err, config, backend) == 1);
    assert(backend.generate_count == 1);
  }
  {
    EchoBackend backend;
    backend.configure_error = yllama::BackendError{"model_load_failed", "bad model"};
    std::istringstream in;
    std::ostringstream out, err;
    assert(yllama::run_stdio(in, out, err, config, backend) == 1);
    assert(count_frames(out.str(), yllama::kFrameError) == 1);
    assert(count_frames(out.str(), yllama::kFrameReady) == 0);
  }
  {
    EchoBackend backend;
    std::string oversized(1, static_cast<char>(yllama::kMessageGenerate));
    oversized += u32(yllama::kMaxFrameBytes + 1);
    std::istringstream in(oversized);
    std::ostringstream out, err;
    assert(yllama::run_stdio(in, out, err, config, backend) == 1);
    assert(backend.generate_count == 0);
  }
  return 0;
}
