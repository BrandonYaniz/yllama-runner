#ifndef YLLAMA_RUNNER_STREAM_FILTER_HPP
#define YLLAMA_RUNNER_STREAM_FILTER_HPP
#include <functional>
#include <string>
#include <string_view>
#include <vector>
namespace yllama {
class StreamFilter {
 public:
  explicit StreamFilter(std::vector<std::string> stops);
  bool push(std::string_view bytes, const std::function<bool(std::string_view)>& emit);
  bool finish(const std::function<bool(std::string_view)>& emit);
  bool stopped() const { return stopped_; }
  const std::string& error() const { return error_; }
 private:
  bool drain(bool final, const std::function<bool(std::string_view)>& emit);
  std::vector<std::string> stops_; std::string pending_; bool stopped_=false; std::string error_;
};
}
#endif
