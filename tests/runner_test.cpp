#include "runner.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <string>

#include "backend.hpp"
#include "frame.hpp"

namespace {

class EchoBackend final : public yllama::Backend {
 public:
  yllama::ConfigureResult configure(const yllama::RunnerConfig& config) override {
    ++configure_count;
    return yllama::ConfigureResult{std::nullopt, config.model_path,
                                   config.context_tokens};
  }

  yllama::GenerateResult generate(
      std::string_view prompt,
      const yllama::GenerateOptions&,
      const yllama::DeltaCallback& on_delta,
      const yllama::CancellationCallback&) override {
    ++generate_count;
    if (prompt == "fail") {
      return yllama::GenerateResult{
          yllama::BackendError{"invalid_prompt", "prompt rejected"}};
    }
    on_delta("echo:");
    on_delta(prompt);
    return yllama::GenerateResult{std::nullopt};
  }

  int configure_count = 0;
  int generate_count = 0;
};

std::string u32_le(unsigned int value) {
  std::string out(4, '\0');
  out[0] = static_cast<char>(value & 0xff);
  out[1] = static_cast<char>((value >> 8) & 0xff);
  out[2] = static_cast<char>((value >> 16) & 0xff);
  out[3] = static_cast<char>((value >> 24) & 0xff);
  return out;
}

std::string prompt_frame(const std::string& prompt) {
  return u32_le(static_cast<unsigned int>(prompt.size())) + prompt;
}

std::string chunk_frame(const std::string& text) {
  return std::string(1, static_cast<char>(yllama::kFrameChunk)) +
         u32_le(static_cast<unsigned int>(text.size())) + text;
}

std::string done_frame() {
  return std::string(1, static_cast<char>(yllama::kFrameDone));
}

std::string error_frame(const std::string& text) {
  std::string out(1, static_cast<char>(yllama::kFrameError));
  out.push_back(static_cast<char>(text.size() & 0xff));
  out.push_back(static_cast<char>((text.size() >> 8) & 0xff));
  out += text;
  return out;
}

}  // namespace

int main() {
  const yllama::RunnerConfig config{"/models/fast/model.gguf", 8192, 4};
  const yllama::GenerateOptions options;

  {
    EchoBackend backend;
    std::istringstream in(prompt_frame("one") + prompt_frame("two"));
    std::ostringstream out;
    std::ostringstream err;

    const int status =
        yllama::run_stdio(in, out, err, config, options, backend);
    assert(status == 0);
    assert(err.str().empty());
    assert(backend.configure_count == 1);
    assert(backend.generate_count == 2);
    assert(out.str() == chunk_frame("echo:") + chunk_frame("one") +
                            done_frame() + chunk_frame("echo:") +
                            chunk_frame("two") + done_frame());
  }

  {
    EchoBackend backend;
    std::istringstream in(prompt_frame("fail"));
    std::ostringstream out;
    std::ostringstream err;

    const int status =
        yllama::run_stdio(in, out, err, config, options, backend);
    assert(status == 0);
    assert(err.str().empty());
    assert(out.str() == error_frame("invalid_prompt: prompt rejected"));
  }

  {
    EchoBackend backend;
    std::istringstream in(std::string("\x03\x00", 2));
    std::ostringstream out;
    std::ostringstream err;

    const int status =
        yllama::run_stdio(in, out, err, config, options, backend);
    assert(status == 1);
    assert(out.str().empty());
    assert(err.str() == "truncated prompt length\n");
  }

  return 0;
}
