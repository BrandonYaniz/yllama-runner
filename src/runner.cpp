#include "runner.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <array>
#include <cerrno>
#include <istream>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <thread>

#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

#include "backend.hpp"
#include "frame.hpp"
#include "stream_filter.hpp"

namespace yllama { namespace {
std::string backend_error_message(const BackendError&e){if(e.code.empty())return e.message;if(e.message.empty())return e.code;return e.code+": "+e.message;}
bool v1_error(std::ostream&o,std::string_view m){bool ok=write_error_frame(o,m);o.flush();return ok;}

int run_v1(std::istream&in,std::ostream&out,std::ostream&err,const RunnerConfig&c,const GenerateOptions&o,Backend&b){
  auto configured=b.configure(c);if(configured.error){err<<backend_error_message(*configured.error)<<'\n';return 1;}
  for(;;){auto f=read_prompt_frame(in);if(f.status==ReadFrameStatus::Eof)return 0;if(f.status==ReadFrameStatus::Invalid){err<<f.error<<'\n';return 1;}
    bool failed=false;auto r=b.generate(f.prompt,o,[&](std::string_view s){if(!s.empty()&&!failed){failed=!write_chunk_frame(out,s);out.flush();}return !failed;},[]{return false;});
    if(failed){err<<"failed to write chunk frame\n";return 1;}if(r.error){if(!v1_error(out,backend_error_message(*r.error)))return 1;continue;}if(!write_done_frame(out))return 1;out.flush();}
}

struct Inbox {std::mutex mu;std::condition_variable cv;std::deque<InputMessage> q;bool eof=false;};
void read_messages(std::istream&in,Inbox&box){for(;;){auto m=read_v2_message(in);const bool done=m.type==kMessageShutdown||m.status==InputStatus::FatalFramingError;{std::lock_guard<std::mutex>l(box.mu);if(m.status==InputStatus::Eof){box.eof=true;box.cv.notify_all();return;}box.q.push_back(std::move(m));}box.cv.notify_all();if(done)return;}}

#ifndef _WIN32
class WakeableFdBuffer final : public std::streambuf {
 public:
  WakeableFdBuffer(int input_fd, int wake_fd)
      : input_fd_(input_fd), wake_fd_(wake_fd) {}

 protected:
  int_type underflow() override {
    if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
    for (;;) {
      std::array<pollfd, 2> descriptors{{{input_fd_, POLLIN, 0},
                                         {wake_fd_, POLLIN, 0}}};
      const int ready = poll(descriptors.data(), descriptors.size(), -1);
      if (ready < 0 && errno == EINTR) continue;
      if (ready <= 0 || (descriptors[1].revents & (POLLIN | POLLHUP | POLLERR)))
        return traits_type::eof();
      if (descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)) {
        const ssize_t count = read(input_fd_, buffer_.data(), buffer_.size());
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return traits_type::eof();
        setg(buffer_.data(), buffer_.data(), buffer_.data() + count);
        return traits_type::to_int_type(*gptr());
      }
    }
  }

 private:
  int input_fd_;
  int wake_fd_;
  std::array<char, 4096> buffer_{};
};
#endif

class InputCoordinator {
 public:
  InputCoordinator(std::istream& input, Inbox& inbox)
      : input_(input), inbox_(inbox) {}
  ~InputCoordinator() { stop(); }

  bool start() {
#ifndef _WIN32
    if (&input_ == &std::cin) {
      if (pipe(wake_pipe_) != 0) return false;
      wakeable_ = true;
      thread_ = std::thread([this] {
        WakeableFdBuffer buffer(STDIN_FILENO, wake_pipe_[0]);
        std::istream stream(&buffer);
        read_messages(stream, inbox_);
      });
      return true;
    }
#endif
    thread_ = std::thread(read_messages, std::ref(input_), std::ref(inbox_));
    return true;
  }

  void stop() {
    if (!thread_.joinable()) return;
#ifndef _WIN32
    if (wakeable_) {
      const char byte = 1;
      static_cast<void>(write(wake_pipe_[1], &byte, 1));
    }
#endif
    thread_.join();
#ifndef _WIN32
    if (wakeable_) {
      close(wake_pipe_[0]);
      close(wake_pipe_[1]);
      wake_pipe_[0] = wake_pipe_[1] = -1;
      wakeable_ = false;
    }
#endif
  }

 private:
  std::istream& input_;
  Inbox& inbox_;
  std::thread thread_;
#ifndef _WIN32
  int wake_pipe_[2] = {-1, -1};
  bool wakeable_ = false;
#endif
};

int run_v2(std::istream&in,std::ostream&out,std::ostream&err,const RunnerConfig&c,Backend&b){
  auto configured=b.configure(c);if(configured.error){write_v2_error(out,configured.error->code,configured.error->message);out.flush();err<<backend_error_message(*configured.error)<<'\n';return 1;}
  if(!write_v2_ready(out,kProtocolV2,c.runner_version,static_cast<std::uint32_t>(configured.context_tokens),kCapabilitiesV2)){err<<"failed to write Ready\n";return 1;}out.flush();
  Inbox box;InputCoordinator reader(in,box);if(!reader.start()){write_v2_error(out,"reader_init_failed","Unable to create interruptible input reader");out.flush();return 1;}bool shutdown=false;int status=0;
  while(!shutdown){InputMessage m;{std::unique_lock<std::mutex>l(box.mu);box.cv.wait(l,[&]{return!box.q.empty()||box.eof;});if(box.q.empty()&&box.eof)break;m=std::move(box.q.front());box.q.pop_front();}
    if(m.status!=InputStatus::Ok){if(!write_v2_error(out,m.error.code,m.error.message)){status=1;break;}out.flush();if(m.status==InputStatus::FatalFramingError){status=1;break;}continue;}
    if(m.type==kMessageShutdown)break;if(m.type==kMessageCancel){if(!write_v2_error(out,"no_active_request","Cancel received with no active Generate")){status=1;break;}out.flush();continue;}
    if(auto invalid=validate_generate_request(m.generate)){if(!write_v2_error(out,invalid->code,invalid->message)){status=1;break;}out.flush();continue;}
    bool cancelled=false,stopped=false,write_failed=false,fatal_input=false;BackendError fatal_input_error;StreamFilter filter(m.generate.options.stop_sequences);
    auto poll_cancel=[&]{std::lock_guard<std::mutex>l(box.mu);for(auto it=box.q.begin();it!=box.q.end();++it){if(it->status==InputStatus::FatalFramingError){fatal_input_error=it->error;box.q.erase(it);fatal_input=true;break;}if(it->type==kMessageCancel){box.q.erase(it);cancelled=true;break;}if(it->type==kMessageShutdown){shutdown=true;cancelled=true;break;}}return fatal_input||cancelled||stopped||write_failed;};
    auto result=b.generate(m.generate.prompt,m.generate.options,[&](std::string_view bytes){if(!filter.push(bytes,[&](std::string_view text){if(text.empty())return true;const bool ok=write_v2_chunk(out,text);out.flush();write_failed=!ok;return ok;})){write_failed=!filter.error().empty();}stopped=filter.stopped();return !poll_cancel();},poll_cancel);
    if(!write_failed&&!stopped&&!cancelled&&!result.error){if(!filter.finish([&](std::string_view text){const bool ok=write_v2_chunk(out,text);out.flush();return ok;})){if(!filter.error().empty())result.error=BackendError{"invalid_backend_utf8",filter.error(),ErrorDisposition::Fatal};else write_failed=true;}stopped=filter.stopped();}
    if(write_failed){err<<"consumer disconnected while writing output\n";status=1;break;}
    if(fatal_input){if(!write_v2_error(out,fatal_input_error.code,fatal_input_error.message)){status=1;break;}out.flush();status=1;break;}
    if(result.error){if(!write_v2_error(out,result.error->code,result.error->message)){status=1;break;}out.flush();if(result.error->disposition==ErrorDisposition::Fatal){status=1;break;}continue;}
    if(cancelled)result.finish_reason=FinishReason::Cancelled;else if(stopped)result.finish_reason=FinishReason::Stop;
    if(!write_v2_completed(out,result)){status=1;break;}out.flush();
  }
  reader.stop();return status;
}
}  // namespace

int run_stdio(std::istream&i,std::ostream&o,std::ostream&e,const RunnerConfig&c,const GenerateOptions&g){auto b=make_default_backend();return run_stdio(i,o,e,c,g,*b);}
int run_stdio(std::istream&i,std::ostream&o,std::ostream&e,const RunnerConfig&c,const GenerateOptions&g,Backend&b){return c.protocol==kProtocolV2?run_v2(i,o,e,c,b):run_v1(i,o,e,c,g,b);}
}  // namespace yllama
