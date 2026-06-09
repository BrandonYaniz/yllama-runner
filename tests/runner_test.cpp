#include "runner.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "backend.hpp"

namespace {

class BlockingBackend final : public yllama::Backend {
 public:
  yllama::ConfigureResult configure(
      const yllama::ConfigureCommand& command) override {
    configured_ = true;
    return yllama::ConfigureResult{std::nullopt, command.model_path,
                                   command.context_tokens};
  }

  yllama::GenerateResult generate(
      const yllama::GenerateCommand&,
      const yllama::DeltaCallback&,
      const yllama::CancellationCallback& is_cancelled) override {
    if (!configured_) {
      return yllama::GenerateResult{
          "error", yllama::Usage{},
          yllama::BackendError{"not_configured",
                               "Backend must be configured before generation."}};
    }

    while (!is_cancelled()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return yllama::GenerateResult{"cancelled", yllama::Usage{}, std::nullopt};
  }

 private:
  bool configured_ = false;
};

class FailingGenerateBackend final : public yllama::Backend {
 public:
  yllama::ConfigureResult configure(
      const yllama::ConfigureCommand& command) override {
    configured_ = true;
    return yllama::ConfigureResult{std::nullopt, command.model_path,
                                   command.context_tokens};
  }

  yllama::GenerateResult generate(
      const yllama::GenerateCommand&,
      const yllama::DeltaCallback&,
      const yllama::CancellationCallback&) override {
    if (!configured_) {
      return yllama::GenerateResult{
          "error", yllama::Usage{},
          yllama::BackendError{"not_configured",
                               "Backend must be configured before generation."}};
    }

    return yllama::GenerateResult{
        "error", yllama::Usage{},
        yllama::BackendError{"invalid_settings",
                             "max_tokens must be greater than zero."}};
  }

 private:
  bool configured_ = false;
};

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::string run_with_backend(yllama::Backend& backend,
                             const std::string& input,
                             int expected_status = 0) {
  std::istringstream in(input);
  std::ostringstream out;
  std::ostringstream err;

  const int status = yllama::run_stdio(in, out, err, backend);
  assert(status == expected_status);
  assert(err.str().empty());
  return out.str();
}

std::string run_with_input(const std::string& input, int expected_status = 0) {
  std::unique_ptr<yllama::Backend> backend = yllama::make_fake_backend();
  return run_with_backend(*backend, input, expected_status);
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
           "\"usage\":{\"input_tokens\":1,\"output_tokens\":2}}\n");
  }

  {
    const std::string out = run_with_input(
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":8,\"stream\":false}}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(out ==
           "{\"type\":\"hello\",\"protocol_version\":1,"
           "\"runner\":\"yllama-runner\","
           "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}\n"
           "{\"type\":\"ready\",\"id\":\"cfg-001\","
           "\"model_path\":\"/models/fast/model.gguf\","
           "\"context_tokens\":8192}\n"
           "{\"type\":\"started\",\"id\":\"req-001\"}\n"
           "{\"type\":\"completed\",\"id\":\"req-001\","
           "\"finish_reason\":\"stop\","
           "\"usage\":{\"input_tokens\":1,\"output_tokens\":2},"
           "\"text\":\"fake response\"}\n");
  }

  {
    const std::string out = run_with_input(
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":8,\"stream\":\"false\"}}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(contains(out,
                    "{\"type\":\"error\",\"id\":\"\","
                    "\"code\":\"invalid_command\","
                    "\"message\":\"stream must be a boolean\"}\n"));
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

  {
    BlockingBackend backend;
    const std::string out = run_with_backend(
        backend,
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":128}}\n"
        "{\"type\":\"cancel\",\"id\":\"req-001\"}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(out ==
           "{\"type\":\"hello\",\"protocol_version\":1,"
           "\"runner\":\"yllama-runner\","
           "\"capabilities\":[\"generate\",\"stream\",\"cancel\"]}\n"
           "{\"type\":\"ready\",\"id\":\"cfg-001\","
           "\"model_path\":\"/models/fast/model.gguf\","
           "\"context_tokens\":8192}\n"
           "{\"type\":\"started\",\"id\":\"req-001\"}\n"
           "{\"type\":\"cancelled\",\"id\":\"req-001\"}\n");
  }

  {
    FailingGenerateBackend backend;
    const std::string out = run_with_backend(
        backend,
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":0}}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(contains(out, "{\"type\":\"started\",\"id\":\"req-001\"}\n"));
    assert(contains(out,
                    "{\"type\":\"error\",\"id\":\"req-001\","
                    "\"code\":\"invalid_settings\","
                    "\"message\":\"max_tokens must be greater than zero.\"}\n"));
  }

  {
    BlockingBackend backend;
    const std::string out = run_with_backend(
        backend,
        "{\"type\":\"configure\",\"id\":\"cfg-001\","
        "\"model_path\":\"/models/fast/model.gguf\","
        "\"context_tokens\":8192,\"threads\":4}\n"
        "{\"type\":\"generate\",\"id\":\"req-001\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Hello\"},"
        "\"settings\":{\"max_tokens\":128}}\n"
        "{\"type\":\"generate\",\"id\":\"req-002\","
        "\"input\":{\"kind\":\"prompt\",\"prompt\":\"Second\"},"
        "\"settings\":{\"max_tokens\":128}}\n"
        "{\"type\":\"configure\",\"id\":\"cfg-002\","
        "\"model_path\":\"/models/other/model.gguf\","
        "\"context_tokens\":4096,\"threads\":2}\n"
        "{\"type\":\"cancel\",\"id\":\"missing\"}\n"
        "{\"type\":\"cancel\",\"id\":\"req-001\"}\n"
        "{\"type\":\"shutdown\",\"id\":\"shutdown-001\"}\n");

    assert(contains(out,
                    "{\"type\":\"error\",\"id\":\"req-002\","
                    "\"code\":\"request_active\","
                    "\"message\":\"Runner already has an active generation request.\"}\n"));
    assert(contains(out,
                    "{\"type\":\"error\",\"id\":\"cfg-002\","
                    "\"code\":\"request_active\","
                    "\"message\":\"Runner already has an active generation request.\"}\n"));
    assert(contains(out,
                    "{\"type\":\"error\",\"id\":\"missing\","
                    "\"code\":\"request_not_active\","
                    "\"message\":\"No active request matched the cancel command.\"}\n"));
    assert(contains(out, "{\"type\":\"cancelled\",\"id\":\"req-001\"}\n"));
  }

  return 0;
}
