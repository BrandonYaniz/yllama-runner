#include "runner.hpp"

#include <cassert>
#include <sstream>
#include <string>

namespace {

std::string run_with_input(const std::string& input, int expected_status = 0) {
  std::istringstream in(input);
  std::ostringstream out;
  std::ostringstream err;

  const int status = yllama::run_stdio(in, out, err);
  assert(status == expected_status);
  assert(err.str().empty());
  return out.str();
}

}  // namespace

int main() {
  {
    const std::string out = run_with_input(
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(out ==
           "{\"type\":\"hello\",\"protocol_version\":1,"
           "\"runner\":\"yllama-runner\","
           "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}\n"
           "{\"type\":\"ready\",\"id\":\"cfg-001\","
           "\"model_path\":\"/models/fast/model.gguf\","
           "\"context_tokens\":8192}\n");
  }

  {
    const std::string out = run_with_input(
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":8}}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(out ==
           "{\"type\":\"hello\",\"protocol_version\":1,"
           "\"runner\":\"yllama-runner\","
           "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}\n"
           "{\"type\":\"ready\",\"id\":\"cfg-001\","
           "\"model_path\":\"/models/fast/model.gguf\","
           "\"context_tokens\":8192}\n"
           "{\"type\":\"started\",\"id\":\"req-001\"}\n"
           "{\"type\":\"delta\",\"id\":\"req-001\",\"text\":\"fake response\"}\n"
           "{\"type\":\"completed\",\"id\":\"req-001\","
           "\"finish_reason\":\"stop\","
           "\"usage\":{\"input_tokens\":0,\"output_tokens\":2}}\n");
  }

  {
    const std::string out = run_with_input(
        "{\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(out ==
           "{\"type\":\"hello\",\"protocol_version\":1,"
           "\"runner\":\"yllama-runner\","
           "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}\n"
           "{\"type\":\"error\",\"id\":\"\","
           "\"code\":\"invalid_json\","
           "\"message\":\"expected string\"}\n");
  }

  {
    const std::string out = run_with_input(
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":8}}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(out ==
           "{\"type\":\"hello\",\"protocol_version\":1,"
           "\"runner\":\"yllama-runner\","
           "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}\n"
           "{\"type\":\"error\",\"id\":\"req-001\","
           "\"code\":\"not_configured\","
           "\"message\":\"Runner must be configured before generation.\"}\n");
  }

  return 0;
}
