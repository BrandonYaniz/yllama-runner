#include "frame.hpp"

#include <algorithm>
#include <array>
#include <istream>
#include <ostream>
#include <utility>

namespace yllama {
namespace {

bool read_exact(std::istream& in, char* data, std::size_t size) {
  in.read(data, static_cast<std::streamsize>(size));
  return static_cast<std::size_t>(in.gcount()) == size;
}

bool write_exact(std::ostream& out, const char* data, std::size_t size) {
  out.write(data, static_cast<std::streamsize>(size));
  return static_cast<bool>(out);
}

std::uint32_t decode_u32_le(const std::array<char, 4>& bytes) {
  return static_cast<std::uint32_t>(
      (static_cast<unsigned char>(bytes[0])) |
      (static_cast<unsigned char>(bytes[1]) << 8) |
      (static_cast<unsigned char>(bytes[2]) << 16) |
      (static_cast<unsigned char>(bytes[3]) << 24));
}

void encode_u32_le(std::uint32_t value, std::array<char, 4>& bytes) {
  bytes[0] = static_cast<char>(value & 0xff);
  bytes[1] = static_cast<char>((value >> 8) & 0xff);
  bytes[2] = static_cast<char>((value >> 16) & 0xff);
  bytes[3] = static_cast<char>((value >> 24) & 0xff);
}

void encode_u16_le(std::uint16_t value, std::array<char, 2>& bytes) {
  bytes[0] = static_cast<char>(value & 0xff);
  bytes[1] = static_cast<char>((value >> 8) & 0xff);
}

}  // namespace

PromptFrame read_prompt_frame(std::istream& in) {
  std::array<char, 4> length_bytes{};
  in.read(length_bytes.data(), static_cast<std::streamsize>(length_bytes.size()));
  const std::streamsize read = in.gcount();
  if (read == 0 && in.eof()) {
    return PromptFrame{ReadFrameStatus::Eof, {}, {}};
  }
  if (read != static_cast<std::streamsize>(length_bytes.size())) {
    return PromptFrame{ReadFrameStatus::Invalid, {}, "truncated prompt length"};
  }

  const std::uint32_t length = decode_u32_le(length_bytes);
  if (length > kMaxPromptBytes) {
    return PromptFrame{ReadFrameStatus::Invalid, {}, "prompt frame too large"};
  }

  std::string prompt(length, '\0');
  if (length > 0 && !read_exact(in, prompt.data(), prompt.size())) {
    return PromptFrame{ReadFrameStatus::Invalid, {}, "truncated prompt payload"};
  }

  return PromptFrame{ReadFrameStatus::Ok, std::move(prompt), {}};
}

bool write_chunk_frame(std::ostream& out, std::string_view text) {
  if (text.size() > kMaxChunkBytes) {
    return false;
  }

  const char tag = static_cast<char>(kFrameChunk);
  std::array<char, 4> length_bytes{};
  encode_u32_le(static_cast<std::uint32_t>(text.size()), length_bytes);

  return write_exact(out, &tag, 1) &&
         write_exact(out, length_bytes.data(), length_bytes.size()) &&
         write_exact(out, text.data(), text.size());
}

bool write_done_frame(std::ostream& out) {
  const char tag = static_cast<char>(kFrameDone);
  return write_exact(out, &tag, 1);
}

bool write_error_frame(std::ostream& out, std::string_view message) {
  const char tag = static_cast<char>(kFrameError);
  const std::size_t size =
      std::min<std::size_t>(message.size(), kMaxErrorBytes);
  std::array<char, 2> length_bytes{};
  encode_u16_le(static_cast<std::uint16_t>(size), length_bytes);

  return write_exact(out, &tag, 1) &&
         write_exact(out, length_bytes.data(), length_bytes.size()) &&
         write_exact(out, message.data(), size);
}

}  // namespace yllama
