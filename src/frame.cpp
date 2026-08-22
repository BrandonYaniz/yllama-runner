#include "frame.hpp"

#include <array>
#include <cstring>
#include <istream>
#include <ostream>
#include <utility>

namespace yllama {
namespace {

bool read_exact(std::istream& in, char* data, std::size_t size) {
  in.read(data, static_cast<std::streamsize>(size));
  return static_cast<std::size_t>(in.gcount()) == size;
}

std::uint32_t read_u32(const char* data) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]) |
      (static_cast<unsigned char>(data[1]) << 8) |
      (static_cast<unsigned char>(data[2]) << 16) |
      (static_cast<unsigned char>(data[3]) << 24));
}

std::uint64_t read_u64(const char* data) {
  std::uint64_t value = 0;
  for (int i = 7; i >= 0; --i) {
    value = (value << 8) | static_cast<unsigned char>(data[i]);
  }
  return value;
}

double read_f64(const char* data) {
  const auto bits = read_u64(data);
  double value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void append_u16(std::string& out, std::uint16_t value) {
  out.push_back(static_cast<char>(value));
  out.push_back(static_cast<char>(value >> 8));
}

void append_u32(std::string& out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<char>(value >> (8 * i)));
  }
}

bool write_envelope(std::ostream& out, std::uint8_t type,
                    std::string_view payload) {
  if (payload.size() > kMaxFrameBytes) return false;
  std::array<char, 5> header{};
  header[0] = static_cast<char>(type);
  const auto size = static_cast<std::uint32_t>(payload.size());
  for (int i = 0; i < 4; ++i) header[i + 1] = static_cast<char>(size >> (8 * i));
  out.write(header.data(), header.size());
  out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  return static_cast<bool>(out);
}

struct Cursor {
  const std::string& data;
  std::size_t offset = 0;

  bool take(std::size_t size, const char*& value) {
    if (size > data.size() - offset) return false;
    value = data.data() + offset;
    offset += size;
    return true;
  }
  bool byte(std::uint8_t& value) {
    const char* data;
    if (!take(1, data)) return false;
    value = static_cast<unsigned char>(*data);
    return true;
  }
  bool u32(std::uint32_t& value) {
    const char* data;
    if (!take(4, data)) return false;
    value = read_u32(data);
    return true;
  }
  bool u64(std::uint64_t& value) {
    const char* data;
    if (!take(8, data)) return false;
    value = read_u64(data);
    return true;
  }
  bool f64(double& value) {
    const char* data;
    if (!take(8, data)) return false;
    value = read_f64(data);
    return true;
  }
  bool string(std::size_t size, std::string& value) {
    const char* data;
    if (!take(size, data)) return false;
    value.assign(data, size);
    return true;
  }
};

InputMessage recoverable(std::string code, std::string message) {
  InputMessage result;
  result.status = InputStatus::RecoverableError;
  result.error = {std::move(code), std::move(message)};
  return result;
}

InputMessage fatal(std::string code, std::string message) {
  InputMessage result;
  result.status = InputStatus::FatalFramingError;
  result.error = {std::move(code), std::move(message), ErrorDisposition::Fatal};
  return result;
}

bool valid_utf8(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size()) {
    const auto c = static_cast<unsigned char>(text[i]);
    std::size_t size;
    if (c < 0x80) size = 1;
    else if (c >= 0xC2 && c <= 0xDF) size = 2;
    else if (c >= 0xE0 && c <= 0xEF) size = 3;
    else if (c >= 0xF0 && c <= 0xF4) size = 4;
    else return false;
    if (i + size > text.size()) return false;
    for (std::size_t j = 1; j < size; ++j) {
      if ((static_cast<unsigned char>(text[i + j]) & 0xC0) != 0x80) return false;
    }
    if (size == 3 && ((c == 0xE0 && static_cast<unsigned char>(text[i + 1]) < 0xA0) ||
                      (c == 0xED && static_cast<unsigned char>(text[i + 1]) >= 0xA0))) return false;
    if (size == 4 && ((c == 0xF0 && static_cast<unsigned char>(text[i + 1]) < 0x90) ||
                      (c == 0xF4 && static_cast<unsigned char>(text[i + 1]) >= 0x90))) return false;
    i += size;
  }
  return true;
}

}  // namespace

InputMessage read_message(std::istream& in) {
  std::array<char, 5> header{};
  in.read(header.data(), header.size());
  if (in.gcount() == 0 && in.eof()) {
    InputMessage result;
    result.status = InputStatus::Eof;
    return result;
  }
  if (in.gcount() != static_cast<std::streamsize>(header.size())) {
    return fatal("malformed_frame", "truncated message envelope");
  }
  const auto type = static_cast<std::uint8_t>(header[0]);
  const auto size = read_u32(header.data() + 1);
  if (size > kMaxFrameBytes) {
    return fatal("frame_too_large", "payload exceeds 32 MiB limit");
  }
  std::string payload(size, '\0');
  if (size && !read_exact(in, payload.data(), size)) {
    return fatal("malformed_frame", "truncated message payload");
  }

  InputMessage result;
  result.type = type;
  if (type == kMessageCancel) {
    if (size) return recoverable("malformed_message", "Cancel payload must be empty");
    result.status = InputStatus::Ok;
    return result;
  }
  if (type != kMessageGenerate) {
    return recoverable("unknown_message_type", "unknown input message type");
  }

  Cursor cursor{payload};
  std::uint8_t mode;
  std::uint8_t stop_count;
  auto& options = result.generate.options;
  std::uint32_t top_k;
  if (!cursor.byte(mode) || !cursor.byte(stop_count) ||
      !cursor.u32(options.max_tokens) || !cursor.f64(options.temperature) ||
      !cursor.f64(options.top_p) || !cursor.u32(top_k) ||
      !cursor.f64(options.min_p) || !cursor.f64(options.presence_penalty) ||
      !cursor.f64(options.repeat_penalty) || !cursor.u64(options.seed)) {
    return recoverable("malformed_generate", "truncated Generate settings");
  }
  if (mode > 1) {
    return recoverable("invalid_tokenization_mode", "tokenization mode must be 0 or 1");
  }
  if (stop_count > kMaxStopCount) {
    return recoverable("too_many_stops", "stop count exceeds 64");
  }
  options.tokenization_mode = static_cast<TokenizationMode>(mode);
  options.top_k = static_cast<std::int32_t>(top_k);

  std::size_t total_stop_bytes = 0;
  for (std::uint8_t i = 0; i < stop_count; ++i) {
    std::uint32_t length;
    if (!cursor.u32(length) || length > kMaxStopBytes ||
        total_stop_bytes + length > kMaxStopBytes) {
      return recoverable("invalid_stop_sequence", "stop strings exceed 64 KiB limit");
    }
    std::string stop;
    if (!cursor.string(length, stop) || stop.empty() || !valid_utf8(stop)) {
      return recoverable("invalid_stop_sequence", "stop strings must be nonempty UTF-8");
    }
    total_stop_bytes += length;
    options.stop_sequences.push_back(std::move(stop));
  }

  const auto prompt_size = payload.size() - cursor.offset;
  if (prompt_size > kMaxPromptBytes) {
    return recoverable("prompt_too_large", "prompt exceeds 16 MiB limit");
  }
  if (!cursor.string(prompt_size, result.generate.prompt) ||
      !valid_utf8(result.generate.prompt)) {
    return recoverable("invalid_prompt_utf8", "prompt must be valid UTF-8");
  }
  result.status = InputStatus::Ok;
  return result;
}

bool write_ready(std::ostream& out) {
  return write_envelope(out, kFrameReady, {});
}

bool write_chunk(std::ostream& out, std::string_view text) {
  return write_envelope(out, kFrameChunk, text);
}

bool write_error(std::ostream& out, std::string_view code,
                 std::string_view message) {
  code = code.substr(0, kMaxErrorCodeBytes);
  message = message.substr(0, kMaxErrorMessageBytes);
  std::string payload;
  append_u16(payload, static_cast<std::uint16_t>(code.size()));
  payload.append(code);
  payload.append(message);
  return write_envelope(out, kFrameError, payload);
}

bool write_completed(std::ostream& out, const GenerateResult& result) {
  std::string payload(1, static_cast<char>(result.finish_reason));
  append_u32(payload, result.input_tokens);
  append_u32(payload, result.output_tokens);
  return write_envelope(out, kFrameCompleted, payload);
}

}  // namespace yllama
