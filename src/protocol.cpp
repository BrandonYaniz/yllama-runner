#include "protocol.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace yllama {
namespace {

void append_json_field(std::ostringstream& out,
                       std::string_view name,
                       std::string_view value) {
  out << '"' << name << "\":\"" << json_escape(value) << '"';
}

}  // namespace

std::string json_escape(std::string_view value) {
  std::ostringstream out;

  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(ch) << std::dec;
        } else {
          out << static_cast<char>(ch);
        }
        break;
    }
  }

  return out.str();
}

std::string hello_event() {
  return "{\"type\":\"hello\",\"protocol_version\":1,"
         "\"runner\":\"yllama-runner\","
         "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}";
}

std::string ready_event(std::string_view id,
                        std::string_view model_path,
                        int context_tokens) {
  std::ostringstream out;
  out << "{\"type\":\"ready\",";
  append_json_field(out, "id", id);
  out << ',';
  append_json_field(out, "model_path", model_path);
  out << ",\"context_tokens\":" << context_tokens << '}';
  return out.str();
}

std::string started_event(std::string_view id) {
  std::ostringstream out;
  out << "{\"type\":\"started\",";
  append_json_field(out, "id", id);
  out << '}';
  return out.str();
}

std::string delta_event(std::string_view id, std::string_view text) {
  std::ostringstream out;
  out << "{\"type\":\"delta\",";
  append_json_field(out, "id", id);
  out << ',';
  append_json_field(out, "text", text);
  out << '}';
  return out.str();
}

std::string completed_event(std::string_view id,
                            std::string_view finish_reason,
                            Usage usage) {
  std::ostringstream out;
  out << "{\"type\":\"completed\",";
  append_json_field(out, "id", id);
  out << ',';
  append_json_field(out, "finish_reason", finish_reason);
  out << ",\"usage\":{\"input_tokens\":" << usage.input_tokens
      << ",\"output_tokens\":" << usage.output_tokens << "}}";
  return out.str();
}

std::string completed_event(std::string_view id,
                            std::string_view finish_reason,
                            Usage usage,
                            std::string_view text) {
  std::ostringstream out;
  out << "{\"type\":\"completed\",";
  append_json_field(out, "id", id);
  out << ',';
  append_json_field(out, "finish_reason", finish_reason);
  out << ",\"usage\":{\"input_tokens\":" << usage.input_tokens
      << ",\"output_tokens\":" << usage.output_tokens << "},";
  append_json_field(out, "text", text);
  out << '}';
  return out.str();
}

std::string cancelled_event(std::string_view id) {
  std::ostringstream out;
  out << "{\"type\":\"cancelled\",";
  append_json_field(out, "id", id);
  out << '}';
  return out.str();
}

std::string error_event(std::string_view id,
                        std::string_view code,
                        std::string_view message) {
  std::ostringstream out;
  out << "{\"type\":\"error\",";
  append_json_field(out, "id", id);
  out << ',';
  append_json_field(out, "code", code);
  out << ',';
  append_json_field(out, "message", message);
  out << '}';
  return out.str();
}

void write_json_line(std::ostream& out, std::string_view event_json) {
  out << event_json << '\n';
}

}  // namespace yllama
