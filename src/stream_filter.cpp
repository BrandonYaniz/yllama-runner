#include "stream_filter.hpp"
#include <algorithm>
namespace yllama { namespace {
std::size_t valid_prefix(std::string_view s, bool final, bool& invalid) {
  std::size_t i=0; invalid=false;
  while(i<s.size()) { unsigned char c=s[i]; std::size_t n=0;
    if(c<0x80)n=1; else if(c>=0xC2&&c<=0xDF)n=2; else if(c>=0xE0&&c<=0xEF)n=3; else if(c>=0xF0&&c<=0xF4)n=4; else {invalid=true;return i;}
    if(i+n>s.size()){if(final)invalid=true;return i;}
    for(std::size_t j=1;j<n;++j)if((static_cast<unsigned char>(s[i+j])&0xC0)!=0x80){invalid=true;return i;}
    if(n==3&&((c==0xE0&&static_cast<unsigned char>(s[i+1])<0xA0)||(c==0xED&&static_cast<unsigned char>(s[i+1])>=0xA0))){invalid=true;return i;}
    if(n==4&&((c==0xF0&&static_cast<unsigned char>(s[i+1])<0x90)||(c==0xF4&&static_cast<unsigned char>(s[i+1])>=0x90))){invalid=true;return i;}
    i+=n;
  } return i;
}
bool prefix(std::string_view whole,std::string_view part){return part.size()<=whole.size()&&whole.substr(0,part.size())==part;}
}
StreamFilter::StreamFilter(std::vector<std::string> s):stops_(std::move(s)){}
bool StreamFilter::push(std::string_view b,const std::function<bool(std::string_view)>& e){if(stopped_)return true;pending_.append(b);return drain(false,e);}
bool StreamFilter::finish(const std::function<bool(std::string_view)>& e){return drain(true,e);}
bool StreamFilter::drain(bool final,const std::function<bool(std::string_view)>& emit){
  if(stopped_)return true; std::size_t match=std::string::npos;for(const auto&s:stops_){auto p=pending_.find(s);if(p<match)match=p;}
  if(match!=std::string::npos){bool bad=false;const auto n=valid_prefix(std::string_view(pending_).substr(0,match),true,bad);if(bad){error_="invalid UTF-8 before stop sequence";return false;}if(n&&!emit(std::string_view(pending_).substr(0,n)))return false;pending_.clear();stopped_=true;return true;}
  bool bad=false;std::size_t complete=valid_prefix(pending_,final,bad);if(bad){error_="backend produced invalid UTF-8";return false;}std::size_t keep=0;
  if(!final)for(std::size_t n=1;n<=complete;++n){auto suffix=std::string_view(pending_).substr(complete-n,n);for(const auto&s:stops_)if(prefix(s,suffix))keep=std::max(keep,n);}
  std::size_t emit_n=complete-keep;if(emit_n&&!emit(std::string_view(pending_).substr(0,emit_n)))return false;pending_.erase(0,emit_n);
  if(final){if(!pending_.empty()){error_="backend ended with incomplete UTF-8";return false;}}return true;
}
}
