#ifndef YLLAMA_RUNNER_PROTOCOL_HPP
#define YLLAMA_RUNNER_PROTOCOL_HPP

#include <iosfwd>
#include <string>
#include <string_view>

namespace yllama {

struct Usage {
  int input_tokens = 0;
  int output_tokens = 0;
};

std::string json_escape(std::string_view value);

std::string hello_event();
std::string ready_event(std::string_view id,
                        std::string_view model_path,
                        int context_tokens);
std::string started_event(std::string_view id);
std::string delta_event(std::string_view id, std::string_view text);
std::string completed_event(std::string_view id,
                            std::string_view finish_reason,
                            Usage usage);
std::string cancelled_event(std::string_view id);
std::string error_event(std::string_view id,
                        std::string_view code,
                        std::string_view message);

void write_json_line(std::ostream& out, std::string_view event_json);

}  // namespace yllama

#endif
