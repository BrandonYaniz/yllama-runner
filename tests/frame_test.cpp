#include "frame.hpp"

#include <cassert>
#include <sstream>
#include <string>

namespace {

std::string u32_le(unsigned int value) {
  std::string out(4, '\0');
  out[0] = static_cast<char>(value & 0xff);
  out[1] = static_cast<char>((value >> 8) & 0xff);
  out[2] = static_cast<char>((value >> 16) & 0xff);
  out[3] = static_cast<char>((value >> 24) & 0xff);
  return out;
}

}  // namespace

int main() {
  {
    std::istringstream in(u32_le(5) + "hello");
    yllama::PromptFrame frame = yllama::read_prompt_frame(in);
    assert(frame.status == yllama::ReadFrameStatus::Ok);
    assert(frame.prompt == "hello");
  }

  {
    std::istringstream in("");
    yllama::PromptFrame frame = yllama::read_prompt_frame(in);
    assert(frame.status == yllama::ReadFrameStatus::Eof);
  }

  {
    std::istringstream in(std::string("\x05\x00", 2));
    yllama::PromptFrame frame = yllama::read_prompt_frame(in);
    assert(frame.status == yllama::ReadFrameStatus::Invalid);
  }

  {
    std::ostringstream out;
    assert(yllama::write_chunk_frame(out, "abc"));
    assert(yllama::write_done_frame(out));
    assert(out.str() == std::string("\x01\x03\x00\x00\x00" "abc" "\x02", 9));
  }

  {
    std::ostringstream out;
    assert(yllama::write_error_frame(out, "bad"));
    assert(out.str() == std::string("\x03\x03\x00" "bad", 6));
  }

  return 0;
}
