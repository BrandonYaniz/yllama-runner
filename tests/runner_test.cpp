#include "runner.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <cstring>

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
    if (!on_delta("echo:") || !on_delta(prompt)) {
      yllama::GenerateResult cancelled;
      cancelled.finish_reason = yllama::FinishReason::Cancelled;
      return cancelled;
    }
    yllama::GenerateResult result;
    result.input_tokens = 1;
    result.output_tokens = 2;
    return result;
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

std::string u16_le(unsigned int value){return std::string{static_cast<char>(value),static_cast<char>(value>>8)};}
std::string u64_le(std::uint64_t value){std::string s;for(int i=0;i<8;++i)s.push_back(static_cast<char>(value>>(i*8)));return s;}
std::string f64_le(double value){std::uint64_t bits;std::memcpy(&bits,&value,8);return u64_le(bits);}
std::string v2_generate(const std::string& prompt){std::string p("\x00\x00",2);p+=u16_le(0)+u32_le(8)+f64_le(0)+f64_le(1)+u32_le(0)+f64_le(0)+f64_le(0)+f64_le(1)+u64_le(1)+u32_le(prompt.size())+prompt;return std::string("\x01",1)+u32_le(p.size())+p;}
std::string v2_shutdown(){return std::string("\x03\x00\x00\x00\x00",5);}

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
    EchoBackend backend; yllama::RunnerConfig v2=config;v2.protocol=2;v2.runner_version="26.07.16.01-Release";
    std::istringstream in(v2_generate("cancel me")+std::string("\x02\x00\x00\x00\x00",5)+v2_generate("after"));std::ostringstream out,err;
    assert(yllama::run_stdio(in,out,err,v2,options,backend)==0);assert(backend.configure_count==1);assert(backend.generate_count==2);
    std::size_t pos=0,completed=0;bool saw_cancelled=false;const auto& bytes=out.str();
    while(pos+5<=bytes.size()){const auto type=static_cast<unsigned char>(bytes[pos]);const auto len=static_cast<unsigned char>(bytes[pos+1])|(static_cast<unsigned char>(bytes[pos+2])<<8)|(static_cast<unsigned char>(bytes[pos+3])<<16)|(static_cast<unsigned char>(bytes[pos+4])<<24);assert(pos+5+len<=bytes.size());if(type==yllama::kFrameCompleted){++completed;if(static_cast<unsigned char>(bytes[pos+5])==3)saw_cancelled=true;}pos+=5+len;}
    assert(completed==2);assert(saw_cancelled);
  }

  {
    EchoBackend backend; yllama::RunnerConfig v2=config;v2.protocol=2;v2.runner_version="26.07.16.01-Release";
    std::istringstream in(v2_generate("one")+v2_shutdown());std::ostringstream out,err;
    assert(yllama::run_stdio(in,out,err,v2,options,backend)==0);assert(backend.configure_count==1);assert(backend.generate_count==1);
    const std::string bytes=out.str();assert(static_cast<unsigned char>(bytes[0])==yllama::kFrameReady);
    assert(bytes.find(std::string(1,static_cast<char>(yllama::kFrameCompleted)))!=std::string::npos);
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
