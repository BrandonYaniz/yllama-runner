#include "backend.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <string>

int main() {
  {
    const auto raw = yllama::tokenization_flags(yllama::TokenizationMode::Raw);
    const auto formatted = yllama::tokenization_flags(yllama::TokenizationMode::Preformatted);
    assert(raw.add_special && !raw.parse_special);
    assert(!formatted.add_special && formatted.parse_special);
  }
  {
    using Options = yllama::GenerateOptions;
    const double invalid_values[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()};
    double Options::* fields[] = {&Options::temperature, &Options::top_p,
                                  &Options::min_p,
                                  &Options::presence_penalty,
                                  &Options::repeat_penalty};
    for (auto field : fields) {
      for (double value : invalid_values) {
        yllama::GenerateRequest request;
        request.prompt = "x";
        request.options.*field = value;
        assert(yllama::validate_generate_request(request).has_value());
      }
    }

    auto valid = [](const yllama::GenerateOptions& options) {
      yllama::GenerateRequest request{"x", options};
      return !yllama::validate_generate_request(request).has_value();
    };
    yllama::GenerateOptions options;
    options.max_tokens = 1; options.temperature = 0; options.top_p = 1;
    options.top_k = 0; options.min_p = 0; options.presence_penalty = -2;
    options.repeat_penalty = std::numeric_limits<double>::min();
    assert(valid(options));
    options.max_tokens = 1000000; options.temperature = 100;
    options.top_k = 1000000; options.min_p = 1; options.presence_penalty = 2;
    options.repeat_penalty = 100;
    assert(valid(options));
    options.max_tokens = 0; assert(!valid(options)); options.max_tokens = 1000001;
    assert(!valid(options)); options.max_tokens = 1;
    options.temperature = -1; assert(!valid(options)); options.temperature = 101;
    assert(!valid(options)); options.temperature = 1;
    options.top_p = 0; assert(!valid(options)); options.top_p = 1.01;
    assert(!valid(options)); options.top_p = 1;
    options.top_k = -1; assert(!valid(options)); options.top_k = 1000001;
    assert(!valid(options)); options.top_k = 1;
    options.min_p = -0.01; assert(!valid(options)); options.min_p = 1.01;
    assert(!valid(options)); options.min_p = 0;
    options.presence_penalty = -2.01; assert(!valid(options));
    options.presence_penalty = 2.01; assert(!valid(options));
    options.presence_penalty = 0;
    options.repeat_penalty = 0; assert(!valid(options));
    options.repeat_penalty = 101; assert(!valid(options));
  }
  auto backend = yllama::make_fake_backend();

  yllama::GenerateOptions options;

  {
    const auto result = backend->generate(
        "Hello", options, [](std::string_view) { return true; }, []() { return false; });
    assert(result.error.has_value());
    assert(result.error->code == "not_configured");
    assert(result.error->disposition == yllama::ErrorDisposition::Fatal);
  }

  {
    yllama::RunnerConfig config;
    config.model_path = "/models/fast/model.gguf";
    config.context_tokens = 8192;
    config.threads = 4;
    const auto result = backend->configure(config);
    assert(!result.error.has_value());
    assert(result.model_path == "/models/fast/model.gguf");
    assert(result.context_tokens == 8192);
  }

  {
    std::string delta;
    const auto result = backend->generate(
        "Hello", options, [&](std::string_view text) { delta.append(text); return true; },
        []() { return false; });
    assert(!result.error.has_value());
    assert(delta == "fake response");
  }

  {
    std::string delta;
    const auto result = backend->generate(
        "Hello", options, [&](std::string_view text) { delta.append(text); return true; },
        []() { return true; });
    assert(!result.error.has_value());
    assert(delta.empty());
  }

  return 0;
}
