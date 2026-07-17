#include "frame.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <cstring>

namespace {

std::string u32_le(unsigned int value) {
  std::string out(4, '\0');
  out[0] = static_cast<char>(value & 0xff);
  out[1] = static_cast<char>((value >> 8) & 0xff);
  out[2] = static_cast<char>((value >> 16) & 0xff);
  out[3] = static_cast<char>((value >> 24) & 0xff);
  return out;
}
std::string u16_le(unsigned int value) { return std::string{static_cast<char>(value), static_cast<char>(value >> 8)}; }
std::string u64_le(std::uint64_t value) { std::string out; for(int i=0;i<8;++i)out.push_back(static_cast<char>(value>>(8*i))); return out; }
std::string f64_le(double value) { std::uint64_t bits; std::memcpy(&bits,&value,8); return u64_le(bits); }
std::string envelope(unsigned char type,const std::string& payload){return std::string(1,static_cast<char>(type))+u32_le(payload.size())+payload;}

}  // namespace

int main() {
  {
    std::istringstream in(u32_le(5) + "hello");
    yllama::PromptFrame frame = yllama::read_prompt_frame(in);
    assert(frame.status == yllama::ReadFrameStatus::Ok);
    assert(frame.prompt == "hello");
  }

  {
    std::string payload("\x01\x00",2); payload+=u16_le(1); payload+=u32_le(7);
    payload+=f64_le(0.25)+f64_le(0.9)+u32_le(20)+f64_le(0.05)+f64_le(0.0)+f64_le(1.1)+u64_le(42);
    payload+=u32_le(3)+"END"+u32_le(2)+"hi";
    std::istringstream in(envelope(yllama::kMessageGenerate,payload));
    auto message=yllama::read_v2_message(in); assert(message.status==yllama::InputStatus::Ok);
    assert(message.generate.prompt=="hi"); assert(message.generate.options.tokenization_mode==yllama::TokenizationMode::Preformatted);
    assert(message.generate.options.max_tokens==7); assert(message.generate.options.seed==42); assert(message.generate.options.stop_sequences[0]=="END");
  }

  {
    std::ostringstream out; yllama::GenerateResult r; r.finish_reason=yllama::FinishReason::Length;r.input_tokens=2;r.output_tokens=3;r.prompt_microseconds=4;r.generation_microseconds=5;
    assert(yllama::write_v2_completed(out,r));
    std::string payload="\x01";payload+=u32_le(2)+u32_le(3)+u64_le(4)+u64_le(5);
    assert(out.str()==envelope(yllama::kFrameCompleted,payload));
  }

  {
    std::istringstream in(std::string("\x01\x01\x00",3)); auto m=yllama::read_v2_message(in);
    assert(m.status==yllama::InputStatus::FatalFramingError);assert(m.error.code=="malformed_frame");
  }

  {
    std::ostringstream out;
    assert(yllama::write_v2_ready(out, 2, "x", 4096, 0x7f));
    const std::string payload = u16_le(2) + u16_le(1) + "x" +
                                u32_le(4096) + u64_le(0x7f);
    assert(out.str() == envelope(yllama::kFrameReady, payload));
  }

  {
    std::ostringstream out;
    assert(yllama::write_v2_error(out, "bad", "oops"));
    const std::string payload = u16_le(3) + "bad" + u16_le(4) + "oops";
    assert(out.str() == envelope(yllama::kFrameError, payload));
  }

  {
    std::istringstream in(std::string("\x01", 1) +
                          u32_le(yllama::kMaxFrameBytes + 1) + "arbitrary");
    auto message = yllama::read_v2_message(in);
    assert(message.status == yllama::InputStatus::FatalFramingError);
    assert(message.error.code == "frame_too_large");
  }

  {
    std::istringstream in(std::string("\x01\x02\x00", 3));
    assert(yllama::read_v2_message(in).status ==
           yllama::InputStatus::FatalFramingError);
  }

  {
    std::istringstream in(std::string("\x01", 1) + u32_le(4) + "xx");
    assert(yllama::read_v2_message(in).status ==
           yllama::InputStatus::FatalFramingError);
  }

  {
    std::string valid("\x00\x00", 2);
    valid += u16_le(0) + u32_le(1) + f64_le(0) + f64_le(1) + u32_le(0) +
             f64_le(0) + f64_le(0) + f64_le(1) + u64_le(1) + u32_le(2) + "ok";
    std::istringstream in(envelope(0x7f, "unknown") +
                          envelope(yllama::kMessageGenerate, valid));
    assert(yllama::read_v2_message(in).status ==
           yllama::InputStatus::RecoverableError);
    auto next = yllama::read_v2_message(in);
    assert(next.status == yllama::InputStatus::Ok && next.generate.prompt == "ok");
  }

  {
    std::string malformed("\x00\x01", 2);
    std::string valid("\x00\x00", 2);
    valid += u16_le(0) + u32_le(1) + f64_le(0) + f64_le(1) + u32_le(0) +
             f64_le(0) + f64_le(0) + f64_le(1) + u64_le(1) + u32_le(2) + "ok";
    std::istringstream in(envelope(yllama::kMessageGenerate, malformed) +
                          envelope(yllama::kMessageGenerate, valid));
    assert(yllama::read_v2_message(in).status ==
           yllama::InputStatus::RecoverableError);
    assert(yllama::read_v2_message(in).status == yllama::InputStatus::Ok);
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
