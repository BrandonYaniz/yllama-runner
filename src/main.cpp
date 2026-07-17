#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#ifndef _WIN32
#include <csignal>
#endif

#include "backend.hpp"
#include "frame.hpp"
#include "runner.hpp"

namespace {
#ifndef YLLAMA_RELEASE_VERSION
#define YLLAMA_RELEASE_VERSION "00.00.00.00-local"
#endif
#ifndef YLLAMA_LLAMA_CPP_REVISION
#define YLLAMA_LLAMA_CPP_REVISION "unknown"
#endif
#ifndef YLLAMA_COMPUTE_BACKENDS
#define YLLAMA_COMPUTE_BACKENDS "CPU"
#endif
#ifndef YLLAMA_TARGET_OS
#define YLLAMA_TARGET_OS "unknown"
#endif
#ifndef YLLAMA_TARGET_ARCH
#define YLLAMA_TARGET_ARCH "unknown"
#endif
constexpr std::string_view kVersion=YLLAMA_RELEASE_VERSION;
void usage(std::ostream&o){o<<"usage: yllama-runner --model PATH --ctx N --threads N [--gpu-layers N] [--protocol 1|2]\n       yllama-runner --version | --build-info\n";}
std::optional<int> integer(std::string_view v){try{std::size_t n=0;int x=std::stoi(std::string(v),&n);if(n!=v.size())return{};return x;}catch(...){return{};}}
std::optional<double> real(std::string_view v){try{std::size_t n=0;double x=std::stod(std::string(v),&n);if(n!=v.size())return{};return x;}catch(...){return{};}}
}
int main(int argc,char**argv){
#ifndef _WIN32
  std::signal(SIGPIPE, SIG_IGN);
#endif
  if(argc==2){std::string_view a(argv[1]);if(a=="--version"){std::cout<<"yllama-runner "<<kVersion<<'\n';return 0;}if(a=="--build-info"){std::cout<<"runner-version: "<<kVersion<<"\nsupported-protocols: 1,2\nllama.cpp-revision: "<<YLLAMA_LLAMA_CPP_REVISION<<"\ncompute-backends: "<<YLLAMA_COMPUTE_BACKENDS<<"\ntarget: "<<YLLAMA_TARGET_OS<<'/'<<YLLAMA_TARGET_ARCH<<'\n';return 0;}if(a=="--help"||a=="-h"){usage(std::cout);return 0;}}
  yllama::RunnerConfig c;c.runner_version=std::string(kVersion);yllama::GenerateOptions o;
  for(int i=1;i<argc;++i){std::string_view a(argv[i]);auto value=[&]()->const char*{if(i+1>=argc){std::cerr<<a<<" requires a value\n";return nullptr;}return argv[++i];};const char*v=nullptr;
    if(a=="--model"){if(!(v=value()))return 2;c.model_path=v;}
    else if(a=="--ctx"){if(!(v=value()))return 2;auto x=integer(v);if(!x){std::cerr<<"--ctx must be an integer\n";return 2;}c.context_tokens=*x;}
    else if(a=="--threads"){if(!(v=value()))return 2;auto x=integer(v);if(!x){std::cerr<<"--threads must be an integer\n";return 2;}c.threads=*x;}
    else if(a=="--gpu-layers"){if(!(v=value()))return 2;auto x=integer(v);if(!x||*x < -1){std::cerr<<"--gpu-layers must be -1, 0, or a positive integer\n";return 2;}c.gpu_layers=*x;}
    else if(a=="--protocol"){if(!(v=value()))return 2;auto x=integer(v);if(!x||(*x!=1&&*x!=2)){std::cerr<<"unsupported protocol: "<<v<<" (supported: 1, 2)\n";return 2;}c.protocol=static_cast<std::uint16_t>(*x);}
    else if(a=="--max-tokens"){if(!(v=value()))return 2;auto x=integer(v);if(!x||*x<=0)return 2;o.max_tokens=static_cast<std::uint32_t>(*x);}
    else if(a=="--temperature"){if(!(v=value()))return 2;auto x=real(v);if(!x)return 2;o.temperature=*x;}
    else if(a=="--top-p"){if(!(v=value()))return 2;auto x=real(v);if(!x)return 2;o.top_p=*x;}
    else{std::cerr<<"unknown argument: "<<a<<'\n';usage(std::cerr);return 2;}}
  if(c.model_path.empty()||c.context_tokens<=0||c.threads<=0){usage(std::cerr);return 2;}
  std::cerr<<"protocol="<<c.protocol<<" context="<<c.context_tokens<<" gpu_layers="<<c.gpu_layers<<'\n';
  return yllama::run_stdio(std::cin,std::cout,std::cerr,c,o);
}
