#include "protocol.hpp"

#include <cassert>
#include <sstream>
#include <string>

int main() {
  assert(yllama::hello_event() ==
         "{\"type\":\"hello\",\"protocol_version\":1,"
         "\"runner\":\"yllama-runner\","
         "\"capabilities\":[\"generate\",\"stream\",\"cancel\",\"output_modes\"]}");

  assert(yllama::ready_event("cfg-001", "/models/fast/model.gguf", 8192) ==
         "{\"type\":\"ready\",\"id\":\"cfg-001\","
         "\"model_path\":\"/models/fast/model.gguf\","
         "\"context_tokens\":8192}");

  assert(yllama::started_event("req-001") ==
         "{\"type\":\"started\",\"id\":\"req-001\"}");

  assert(yllama::delta_event("req-001", "line 1\n\"quoted\"") ==
         "{\"type\":\"delta\",\"id\":\"req-001\","
         "\"text\":\"line 1\\n\\\"quoted\\\"\"}");

  assert(yllama::completed_event("req-001", "stop", {42, 91}) ==
         "{\"type\":\"completed\",\"id\":\"req-001\","
         "\"finish_reason\":\"stop\","
         "\"usage\":{\"input_tokens\":42,\"output_tokens\":91}}");

  assert(yllama::completed_event("req-001", "stop", {42, 91},
                                 "line 1\n\"quoted\"") ==
         "{\"type\":\"completed\",\"id\":\"req-001\","
         "\"finish_reason\":\"stop\","
         "\"usage\":{\"input_tokens\":42,\"output_tokens\":91},"
         "\"text\":\"line 1\\n\\\"quoted\\\"\"}");

  assert(yllama::cancelled_event("req-001") ==
         "{\"type\":\"cancelled\",\"id\":\"req-001\"}");

  assert(yllama::error_event("req-001", "generation_failed",
                             "Generation failed.") ==
         "{\"type\":\"error\",\"id\":\"req-001\","
         "\"code\":\"generation_failed\","
         "\"message\":\"Generation failed.\"}");

  std::ostringstream out;
  yllama::write_json_line(out, yllama::started_event("req-002"));
  assert(out.str() == "{\"type\":\"started\",\"id\":\"req-002\"}\n");

  return 0;
}
