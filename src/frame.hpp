#ifndef YLLAMA_RUNNER_FRAME_HPP
#define YLLAMA_RUNNER_FRAME_HPP

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace yllama {

constexpr std::uint8_t kFrameChunk = 0x01;
constexpr std::uint8_t kFrameDone = 0x02;
constexpr std::uint8_t kFrameError = 0x03;
constexpr std::uint32_t kMaxPromptBytes = 16 * 1024 * 1024;
constexpr std::uint32_t kMaxChunkBytes = 16 * 1024 * 1024;
constexpr std::uint16_t kMaxErrorBytes = 64 * 1024 - 1;

enum class ReadFrameStatus {
  Ok,
  Eof,
  Invalid,
};

struct PromptFrame {
  ReadFrameStatus status = ReadFrameStatus::Invalid;
  std::string prompt;
  std::string error;
};

PromptFrame read_prompt_frame(std::istream& in);
bool write_chunk_frame(std::ostream& out, std::string_view text);
bool write_done_frame(std::ostream& out);
bool write_error_frame(std::ostream& out, std::string_view message);

}  // namespace yllama

#endif
