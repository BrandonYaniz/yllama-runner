#ifndef YLLAMA_RUNNER_FRAME_HPP
#define YLLAMA_RUNNER_FRAME_HPP

#include <cstdint>
#include <iosfwd>
#include <string_view>

#include "backend.hpp"

namespace yllama {

constexpr std::uint8_t kMessageGenerate = 0x01;
constexpr std::uint8_t kMessageCancel = 0x02;
constexpr std::uint8_t kFrameChunk = 0x01;
constexpr std::uint8_t kFrameError = 0x03;
constexpr std::uint8_t kFrameCompleted = 0x04;
constexpr std::uint8_t kFrameReady = 0x10;
constexpr std::uint32_t kMaxFrameBytes = 32 * 1024 * 1024;
constexpr std::uint32_t kMaxPromptBytes = 16 * 1024 * 1024;
constexpr std::uint32_t kMaxStopBytes = 64 * 1024;
constexpr std::uint8_t kMaxStopCount = 64;
constexpr std::uint16_t kMaxErrorCodeBytes = 128;
constexpr std::uint16_t kMaxErrorMessageBytes = 16 * 1024;

enum class InputStatus { Ok, Eof, RecoverableError, FatalFramingError };

struct InputMessage {
  InputStatus status = InputStatus::FatalFramingError;
  std::uint8_t type = 0;
  GenerateRequest generate;
  BackendError error;
};

InputMessage read_message(std::istream& in);
bool write_ready(std::ostream& out);
bool write_chunk(std::ostream& out, std::string_view text);
bool write_error(std::ostream& out, std::string_view code,
                 std::string_view message);
bool write_completed(std::ostream& out, const GenerateResult& result);

}  // namespace yllama
#endif
