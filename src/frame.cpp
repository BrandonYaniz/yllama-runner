#include "frame.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>

namespace yllama { namespace {
bool read_exact(std::istream& in, char* p, std::size_t n) {
  in.read(p, static_cast<std::streamsize>(n));
  return static_cast<std::size_t>(in.gcount()) == n;
}
bool write_exact(std::ostream& out, const char* p, std::size_t n) {
  out.write(p, static_cast<std::streamsize>(n)); return static_cast<bool>(out);
}
std::uint16_t u16(const char* p) { return static_cast<std::uint16_t>(static_cast<unsigned char>(p[0]) | (static_cast<unsigned char>(p[1]) << 8)); }
std::uint32_t u32(const char* p) { return static_cast<std::uint32_t>(static_cast<unsigned char>(p[0]) | (static_cast<unsigned char>(p[1]) << 8) | (static_cast<unsigned char>(p[2]) << 16) | (static_cast<unsigned char>(p[3]) << 24)); }
std::uint64_t u64(const char* p) { std::uint64_t v=0; for(int i=7;i>=0;--i) v=(v<<8)|static_cast<unsigned char>(p[i]); return v; }
double f64(const char* p) { const auto bits=u64(p); double v; std::memcpy(&v,&bits,8); return v; }
void put16(std::string& s, std::uint16_t v) { s.push_back(static_cast<char>(v)); s.push_back(static_cast<char>(v>>8)); }
void put32(std::string& s, std::uint32_t v) { for(int i=0;i<4;++i)s.push_back(static_cast<char>(v>>(8*i))); }
void put64(std::string& s, std::uint64_t v) { for(int i=0;i<8;++i)s.push_back(static_cast<char>(v>>(8*i))); }
bool envelope(std::ostream& out, std::uint8_t type, std::string_view payload) {
  if(payload.size()>kMaxFrameBytes)return false; std::string h; h.push_back(static_cast<char>(type)); put32(h,static_cast<std::uint32_t>(payload.size()));
  return write_exact(out,h.data(),h.size())&&write_exact(out,payload.data(),payload.size());
}
struct Cursor { const std::string& s; std::size_t p=0;
  bool take(std::size_t n,const char*& q){if(n>s.size()-p)return false;q=s.data()+p;p+=n;return true;}
  bool byte(std::uint8_t& v){const char*q;if(!take(1,q))return false;v=static_cast<unsigned char>(*q);return true;}
  bool word(std::uint16_t& v){const char*q;if(!take(2,q))return false;v=u16(q);return true;}
  bool dword(std::uint32_t& v){const char*q;if(!take(4,q))return false;v=u32(q);return true;}
  bool qword(std::uint64_t& v){const char*q;if(!take(8,q))return false;v=u64(q);return true;}
  bool real(double& v){const char*q;if(!take(8,q))return false;v=f64(q);return true;}
  bool str(std::uint32_t n,std::string& v){const char*q;if(!take(n,q))return false;v.assign(q,n);return true;}
};
InputMessage recoverable(std::string code,std::string msg){InputMessage m;m.status=InputStatus::RecoverableError;m.error={std::move(code),std::move(msg)};return m;}
InputMessage fatal(std::string code,std::string msg){InputMessage m;m.status=InputStatus::FatalFramingError;m.error={std::move(code),std::move(msg),ErrorDisposition::Fatal};return m;}
bool valid_utf8(std::string_view s){std::size_t i=0;while(i<s.size()){unsigned char c=s[i];std::size_t n;if(c<0x80)n=1;else if(c>=0xC2&&c<=0xDF)n=2;else if(c>=0xE0&&c<=0xEF)n=3;else if(c>=0xF0&&c<=0xF4)n=4;else return false;if(i+n>s.size())return false;for(std::size_t j=1;j<n;++j)if((static_cast<unsigned char>(s[i+j])&0xC0)!=0x80)return false;if(n==3&&((c==0xE0&&static_cast<unsigned char>(s[i+1])<0xA0)||(c==0xED&&static_cast<unsigned char>(s[i+1])>=0xA0)))return false;if(n==4&&((c==0xF0&&static_cast<unsigned char>(s[i+1])<0x90)||(c==0xF4&&static_cast<unsigned char>(s[i+1])>=0x90)))return false;i+=n;}return true;}
}  // namespace

PromptFrame read_prompt_frame(std::istream& in) {
  std::array<char,4>b{}; in.read(b.data(),4); if(in.gcount()==0&&in.eof())return{ReadFrameStatus::Eof,{},{}};
  if(in.gcount()!=4)return{ReadFrameStatus::Invalid,{},"truncated prompt length"}; const auto n=u32(b.data());
  if(n>kMaxPromptBytes)return{ReadFrameStatus::Invalid,{},"prompt frame too large"}; std::string p(n,'\0');
  if(n&&!read_exact(in,p.data(),n))return{ReadFrameStatus::Invalid,{},"truncated prompt payload"}; return{ReadFrameStatus::Ok,std::move(p),{}};
}

InputMessage read_v2_message(std::istream& in) {
  std::array<char,5>h{}; in.read(h.data(),5); if(in.gcount()==0&&in.eof()){InputMessage m;m.status=InputStatus::Eof;return m;}
  if(in.gcount()!=5)return fatal("malformed_frame","truncated message envelope"); const std::uint8_t type=static_cast<unsigned char>(h[0]); const auto n=u32(h.data()+1);
  if(n>kMaxFrameBytes)return fatal("frame_too_large","payload exceeds 32 MiB limit"); std::string p(n,'\0');
  if(n&&!read_exact(in,p.data(),n))return fatal("malformed_frame","truncated message payload"); InputMessage m;m.type=type;
  if(type==kMessageCancel||type==kMessageShutdown){if(n)return recoverable("malformed_message","Cancel and Shutdown payloads must be empty");m.status=InputStatus::Ok;return m;}
  if(type!=kMessageGenerate)return recoverable("unknown_message_type","unknown input message type"); Cursor c{p};std::uint8_t mode,flags;std::uint16_t stops;std::uint32_t max;
  if(!c.byte(mode)||!c.byte(flags)||!c.word(stops)||!c.dword(max))return recoverable("malformed_generate","truncated Generate settings");
  if(flags!=0)return recoverable("invalid_reserved","Generate reserved flags must be zero"); if(mode>1)return recoverable("invalid_tokenization_mode","tokenization_mode must be 0 or 1"); if(stops>kMaxStopCount)return recoverable("too_many_stops","stop_count exceeds 64");
  auto&o=m.generate.options;o.tokenization_mode=static_cast<TokenizationMode>(mode);o.max_tokens=max;std::uint32_t topk;
  if(!c.real(o.temperature)||!c.real(o.top_p)||!c.dword(topk)||!c.real(o.min_p)||!c.real(o.presence_penalty)||!c.real(o.repeat_penalty)||!c.qword(o.seed))return recoverable("malformed_generate","truncated Generate sampling settings");o.top_k=static_cast<std::int32_t>(topk);
  std::size_t total_stop=0;for(std::uint16_t i=0;i<stops;++i){std::uint32_t len;if(!c.dword(len)||len>kMaxStopBytes||total_stop+len>kMaxStopBytes)return recoverable("invalid_stop_sequence","stop strings exceed 64 KiB aggregate limit");std::string s;if(!c.str(len,s)||s.empty()||!valid_utf8(s))return recoverable("invalid_stop_sequence","stop strings must be nonempty valid UTF-8");total_stop+=len;o.stop_sequences.push_back(std::move(s));}
  std::uint32_t plen;if(!c.dword(plen)||plen>kMaxPromptBytes||!c.str(plen,m.generate.prompt)||c.p!=p.size())return recoverable("malformed_generate","invalid prompt length or trailing bytes");if(!valid_utf8(m.generate.prompt))return recoverable("invalid_prompt_utf8","prompt must be valid UTF-8");m.status=InputStatus::Ok;return m;
}

bool write_chunk_frame(std::ostream& out,std::string_view t){if(t.size()>kMaxFrameBytes)return false;char tag=kFrameChunk;std::string n;put32(n,static_cast<std::uint32_t>(t.size()));return write_exact(out,&tag,1)&&write_exact(out,n.data(),4)&&write_exact(out,t.data(),t.size());}
bool write_done_frame(std::ostream& out){char t=kFrameDoneV1;return write_exact(out,&t,1);}
bool write_error_frame(std::ostream& out,std::string_view m){char t=kFrameError;const auto n=static_cast<std::uint16_t>(std::min<std::size_t>(m.size(),65535));std::string h;put16(h,n);return write_exact(out,&t,1)&&write_exact(out,h.data(),2)&&write_exact(out,m.data(),n);}
bool write_v2_chunk(std::ostream& out,std::string_view t){std::string p;put32(p,static_cast<std::uint32_t>(t.size()));p.append(t);return envelope(out,kFrameChunk,p);}
bool write_v2_error(std::ostream& out,std::string_view c,std::string_view m){c=c.substr(0,kMaxErrorCodeBytes);m=m.substr(0,kMaxErrorMessageBytes);std::string p;put16(p,static_cast<std::uint16_t>(c.size()));p.append(c);put16(p,static_cast<std::uint16_t>(m.size()));p.append(m);return envelope(out,kFrameError,p);}
bool write_v2_completed(std::ostream& out,const GenerateResult&r){std::string p;p.push_back(static_cast<char>(r.finish_reason));put32(p,r.input_tokens);put32(p,r.output_tokens);put64(p,r.prompt_microseconds);put64(p,r.generation_microseconds);return envelope(out,kFrameCompleted,p);}
bool write_v2_ready(std::ostream& out,std::uint16_t v,std::string_view rv,std::uint32_t ctx,std::uint64_t caps){if(rv.size()>65535)return false;std::string p;put16(p,v);put16(p,static_cast<std::uint16_t>(rv.size()));p.append(rv);put32(p,ctx);put64(p,caps);return envelope(out,kFrameReady,p);}
}  // namespace yllama
