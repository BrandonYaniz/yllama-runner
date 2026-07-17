#include "runner.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <thread>

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
void read_messages(std::istream&in,Inbox&box){for(;;){auto m=read_v2_message(in);const bool done=m.type==kMessageShutdown;{std::lock_guard<std::mutex>l(box.mu);if(m.status==ReadFrameStatus::Eof){box.eof=true;box.cv.notify_all();return;}box.q.push_back(std::move(m));}box.cv.notify_all();if(done)return;}}

int run_v2(std::istream&in,std::ostream&out,std::ostream&err,const RunnerConfig&c,Backend&b){
  auto configured=b.configure(c);if(configured.error){write_v2_error(out,configured.error->code,configured.error->message);out.flush();err<<backend_error_message(*configured.error)<<'\n';return 1;}
  if(!write_v2_ready(out,kProtocolV2,c.runner_version,static_cast<std::uint32_t>(configured.context_tokens),kCapabilitiesV2)){err<<"failed to write Ready\n";return 1;}out.flush();
  Inbox box;std::thread reader(read_messages,std::ref(in),std::ref(box));bool shutdown=false;int status=0;
  while(!shutdown){InputMessage m;{std::unique_lock<std::mutex>l(box.mu);box.cv.wait(l,[&]{return!box.q.empty()||box.eof;});if(box.q.empty()&&box.eof)break;m=std::move(box.q.front());box.q.pop_front();}
    if(m.status!=ReadFrameStatus::Ok){if(!write_v2_error(out,m.error.code,m.error.message)){status=1;break;}out.flush();continue;}
    if(m.type==kMessageShutdown)break;if(m.type==kMessageCancel){if(!write_v2_error(out,"no_active_request","Cancel received with no active Generate")){status=1;break;}out.flush();continue;}
    if(auto invalid=validate_generate_request(m.generate)){if(!write_v2_error(out,invalid->code,invalid->message)){status=1;break;}out.flush();continue;}
    bool cancelled=false,stopped=false,write_failed=false;StreamFilter filter(m.generate.options.stop_sequences);
    auto poll_cancel=[&]{std::lock_guard<std::mutex>l(box.mu);for(auto it=box.q.begin();it!=box.q.end();++it){if(it->type==kMessageCancel){box.q.erase(it);cancelled=true;break;}if(it->type==kMessageShutdown){shutdown=true;cancelled=true;break;}}return cancelled||stopped||write_failed;};
    auto result=b.generate(m.generate.prompt,m.generate.options,[&](std::string_view bytes){if(!filter.push(bytes,[&](std::string_view text){if(text.empty())return true;const bool ok=write_v2_chunk(out,text);out.flush();write_failed=!ok;return ok;})){write_failed=!filter.error().empty();}stopped=filter.stopped();return !poll_cancel();},poll_cancel);
    if(!write_failed&&!stopped&&!cancelled&&!result.error){if(!filter.finish([&](std::string_view text){const bool ok=write_v2_chunk(out,text);out.flush();return ok;})){if(!filter.error().empty())result.error=BackendError{"invalid_backend_utf8",filter.error()};else write_failed=true;}stopped=filter.stopped();}
    if(write_failed){err<<"consumer disconnected while writing output\n";status=1;break;}
    if(result.error){if(!write_v2_error(out,result.error->code,result.error->message)){status=1;break;}out.flush();continue;}
    if(cancelled)result.finish_reason=FinishReason::Cancelled;else if(stopped)result.finish_reason=FinishReason::Stop;
    if(!write_v2_completed(out,result)){status=1;break;}out.flush();
  }
  if(reader.joinable())reader.join();return status;
}
}  // namespace

int run_stdio(std::istream&i,std::ostream&o,std::ostream&e,const RunnerConfig&c,const GenerateOptions&g){auto b=make_default_backend();return run_stdio(i,o,e,c,g,*b);}
int run_stdio(std::istream&i,std::ostream&o,std::ostream&e,const RunnerConfig&c,const GenerateOptions&g,Backend&b){return c.protocol==kProtocolV2?run_v2(i,o,e,c,b):run_v1(i,o,e,c,g,b);}
}  // namespace yllama
