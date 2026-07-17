#include "stream_filter.hpp"
#include <cassert>
#include <string>
#include <vector>
int main(){
  auto split=[](const std::string& value){for(std::size_t cut=1;cut<value.size();++cut){yllama::StreamFilter f({});std::string out;assert(f.push(std::string_view(value).substr(0,cut),[&](std::string_view s){out+=s;return true;}));assert(f.push(std::string_view(value).substr(cut),[&](std::string_view s){out+=s;return true;}));assert(f.finish([&](std::string_view s){out+=s;return true;}));assert(out==value);}};
  split("\xC2\xA2");split("\xE2\x82\xAC");split("\xF0\x9F\x98\x80");
  {yllama::StreamFilter f({"STOP"});std::string out;assert(f.push("hello ST",[&](std::string_view s){out+=s;return true;}));assert(f.push("OP leaked",[&](std::string_view s){out+=s;return true;}));assert(f.stopped());assert(out=="hello ");}
  {yllama::StreamFilter f({"\xE2\x98\x83stop"});std::string out;assert(f.push("hello \xE2",[&](std::string_view s){out+=s;return true;}));assert(f.push("\x98\x83st",[&](std::string_view s){out+=s;return true;}));assert(f.push("op hidden",[&](std::string_view s){out+=s;return true;}));assert(f.stopped());assert(out=="hello ");}
  {yllama::StreamFilter f({});assert(f.push("\xF0\x9F",[](std::string_view){return true;}));assert(!f.finish([](std::string_view){return true;}));assert(!f.error().empty());}
}
