#ifndef YLLAMA_RUNNER_FRAME_HPP
#define YLLAMA_RUNNER_FRAME_HPP

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "backend.hpp"

namespace yllama {

constexpr std::uint16_t kProtocolV1 = 1;
constexpr std::uint16_t kProtocolV2 = 2;
constexpr std::uint8_t kMessageGenerate = 0x01;
constexpr std::uint8_t kMessageCancel = 0x02;
constexpr std::uint8_t kMessageShutdown = 0x03;
constexpr std::uint8_t kFrameChunk = 0x01;
constexpr std::uint8_t kFrameDoneV1 = 0x02;
constexpr std::uint8_t kFrameDone = kFrameDoneV1;
constexpr std::uint8_t kFrameError = 0x03;
constexpr std::uint8_t kFrameCompleted = 0x04;
constexpr std::uint8_t kFrameReady = 0x10;
constexpr std::uint32_t kMaxFrameBytes = 32 * 1024 * 1024;
constexpr std::uint32_t kMaxPromptBytes = 16 * 1024 * 1024;
constexpr std::uint32_t kMaxStopBytes = 64 * 1024;
constexpr std::uint16_t kMaxStopCount = 64;
constexpr std::uint16_t kMaxErrorCodeBytes = 128;
constexpr std::uint16_t kMaxErrorMessageBytes = 16 * 1024;

constexpr std::uint64_t kCapabilityStructuredCompletion = 1ULL << 0;
constexpr std::uint64_t kCapabilityPerRequestSampling = 1ULL << 1;
constexpr std::uint64_t kCapabilityTokenizationModes = 1ULL << 2;
constexpr std::uint64_t kCapabilityCancellation = 1ULL << 3;
constexpr std::uint64_t kCapabilityStopSequences = 1ULL << 4;
constexpr std::uint64_t kCapabilityUsageCounters = 1ULL << 5;
constexpr std::uint64_t kCapabilityTimingMetadata = 1ULL << 6;
constexpr std::uint64_t kCapabilitiesV2 = (1ULL << 7) - 1;

enum class ReadFrameStatus { Ok, Eof, Invalid };
enum class InputStatus { Ok, Eof, RecoverableError, FatalFramingError };

struct PromptFrame {
  ReadFrameStatus status = ReadFrameStatus::Invalid;
  std::string prompt;
  std::string error;
};

struct InputMessage {
  InputStatus status = InputStatus::FatalFramingError;
  std::uint8_t type = 0;
  GenerateRequest generate;
  BackendError error;
};

PromptFrame read_prompt_frame(std::istream& in);
InputMessage read_v2_message(std::istream& in);
bool write_chunk_frame(std::ostream& out, std::string_view text);
bool write_done_frame(std::ostream& out);
bool write_v2_chunk(std::ostream& out, std::string_view text);
bool write_v2_error(std::ostream& out, std::string_view code,
                    std::string_view message);
bool write_v2_completed(std::ostream& out, const GenerateResult& result);
bool write_v2_ready(std::ostream& out, std::uint16_t protocol,
                    std::string_view version, std::uint32_t context_size,
                    std::uint64_t capabilities);
bool write_error_frame(std::ostream& out, std::string_view message);

}  // namespace yllama
#endif
