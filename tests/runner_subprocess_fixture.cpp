#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "backend.hpp"
#include "runner.hpp"

#ifndef _WIN32
#include <csignal>
#endif

namespace {
class FixtureBackend final : public yllama::Backend {
 public:
  std::optional<yllama::BackendError> configure(const yllama::RunnerConfig&) override {
    return std::nullopt;
  }

  yllama::GenerateResult generate(
      std::string_view prompt, const yllama::GenerateOptions&,
      const yllama::DeltaCallback& on_delta,
      const yllama::CancellationCallback& is_cancelled) override {
    if (prompt == "fatal") {
      yllama::GenerateResult result;
      result.error = yllama::BackendError{"fixture_fatal", "fatal fixture error",
                                          yllama::ErrorDisposition::Fatal};
      return result;
    }
    if (prompt == "recoverable") {
      yllama::GenerateResult result;
      result.error = yllama::BackendError{"fixture_recoverable",
                                          "recoverable fixture error"};
      return result;
    }
    yllama::GenerateResult result;
    result.input_tokens = 2;
    if (prompt == "slow") {
      for (int i = 0; i < 10000; ++i) {
        if (is_cancelled() || !on_delta("x")) {
          result.finish_reason = yllama::FinishReason::Cancelled;
          return result;
        }
        ++result.output_tokens;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } else {
      if (!on_delta("fixture output")) {
        result.finish_reason = yllama::FinishReason::Cancelled;
        return result;
      }
      result.output_tokens = 2;
    }
    result.finish_reason = yllama::FinishReason::Length;
    return result;
  }
};
}

int main() {
#ifndef _WIN32
  std::signal(SIGPIPE, SIG_IGN);
#endif
  yllama::RunnerConfig config;
  config.model_path = "fixture.gguf";
  config.context_tokens = 2048;
  config.threads = 1;
  FixtureBackend backend;
  return yllama::run_stdio(std::cin, std::cout, std::cerr, config, backend);
}
