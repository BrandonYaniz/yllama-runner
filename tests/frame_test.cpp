#include "frame.hpp"

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>

namespace {
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
std::string generate(const std::string& prompt, unsigned char mode = 0,
                     const std::string& stop = {}) {
  std::string payload(1, static_cast<char>(mode));
  payload.push_back(stop.empty() ? 0 : 1);
  payload += u32(7) + f64(0.25) + f64(0.9) + u32(20) + f64(0.05) +
             f64(0.0) + f64(1.1) + u64(42);
  if (!stop.empty()) payload += u32(stop.size()) + stop;
  payload += prompt;
  return envelope(yllama::kMessageGenerate, payload);
}
}  // namespace

int main() {
  {
    std::istringstream in(generate("hi", 1, "END"));
    auto message = yllama::read_message(in);
    assert(message.status == yllama::InputStatus::Ok);
    assert(message.generate.prompt == "hi");
    assert(message.generate.options.tokenization_mode == yllama::TokenizationMode::Preformatted);
    assert(message.generate.options.max_tokens == 7);
    assert(message.generate.options.seed == 42);
    assert(message.generate.options.stop_sequences[0] == "END");
  }
  {
    std::ostringstream out;
    assert(yllama::write_ready(out));
    assert(out.str() == envelope(yllama::kFrameReady));
  }
  {
    std::ostringstream out;
    assert(yllama::write_chunk(out, "hi"));
    assert(out.str() == envelope(yllama::kFrameChunk, "hi"));
  }
  {
    yllama::GenerateResult result;
    result.finish_reason = yllama::FinishReason::Length;
    result.input_tokens = 2;
    result.output_tokens = 3;
    std::ostringstream out;
    assert(yllama::write_completed(out, result));
    assert(out.str() == envelope(yllama::kFrameCompleted,
                                 std::string("\x01", 1) + u32(2) + u32(3)));
  }
  {
    std::ostringstream out;
    assert(yllama::write_error(out, "bad", "oops"));
    assert(out.str() == envelope(yllama::kFrameError,
                                 std::string("\x03\x00", 2) + "bad" + "oops"));
  }
  {
    std::istringstream in(std::string("\x01\x01\x00", 3));
    assert(yllama::read_message(in).status == yllama::InputStatus::FatalFramingError);
  }
  {
    std::istringstream in(std::string("\x01", 1) +
                          u32(yllama::kMaxFrameBytes + 1));
    auto message = yllama::read_message(in);
    assert(message.status == yllama::InputStatus::FatalFramingError);
    assert(message.error.code == "frame_too_large");
  }
  {
    std::istringstream in(envelope(0x7f, "unknown") + generate("ok"));
    assert(yllama::read_message(in).status == yllama::InputStatus::RecoverableError);
    assert(yllama::read_message(in).generate.prompt == "ok");
  }
  {
    std::istringstream in("");
    assert(yllama::read_message(in).status == yllama::InputStatus::Eof);
  }
  return 0;
}
