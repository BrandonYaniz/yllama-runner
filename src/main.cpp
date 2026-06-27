#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "backend.hpp"
#include "runner.hpp"

namespace {

#ifndef YLLAMA_RELEASE_VERSION
#define YLLAMA_RELEASE_VERSION "0.0.0-local"
#endif

constexpr std::string_view kVersion = YLLAMA_RELEASE_VERSION;

void print_usage(std::ostream& out) {
  out << "usage: yllama-runner --model PATH --ctx N --threads N "
         "[--max-tokens N] [--temperature N] [--top-p N]\n"
         "       yllama-runner --version\n";
}

std::optional<int> parse_int(std::string_view value) {
  try {
    std::size_t consumed = 0;
    const int parsed = std::stoi(std::string(value), &consumed);
    if (consumed != value.size()) {
      return std::nullopt;
    }
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<double> parse_double(std::string_view value) {
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(std::string(value), &consumed);
    if (consumed != value.size()) {
      return std::nullopt;
    }
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2) {
    const std::string_view arg(argv[1]);
    if (arg == "--version") {
      std::cout << "yllama-runner " << kVersion << '\n';
      return 0;
    }

    if (arg == "--help" || arg == "-h") {
      print_usage(std::cout);
      return 0;
    }
  }

  yllama::RunnerConfig config;
  yllama::GenerateOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    auto require_value = [&](std::string_view name) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << name << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--model") {
      const char* value = require_value(arg);
      if (value == nullptr) {
        return 2;
      }
      config.model_path = value;
      continue;
    }
    if (arg == "--ctx") {
      const char* value = require_value(arg);
      if (value == nullptr) {
        return 2;
      }
      std::optional<int> parsed = parse_int(value);
      if (!parsed) {
        std::cerr << "--ctx must be an integer\n";
        return 2;
      }
      config.context_tokens = *parsed;
      continue;
    }
    if (arg == "--threads") {
      const char* value = require_value(arg);
      if (value == nullptr) {
        return 2;
      }
      std::optional<int> parsed = parse_int(value);
      if (!parsed) {
        std::cerr << "--threads must be an integer\n";
        return 2;
      }
      config.threads = *parsed;
      continue;
    }
    if (arg == "--max-tokens") {
      const char* value = require_value(arg);
      if (value == nullptr) {
        return 2;
      }
      std::optional<int> parsed = parse_int(value);
      if (!parsed) {
        std::cerr << "--max-tokens must be an integer\n";
        return 2;
      }
      options.max_tokens = *parsed;
      continue;
    }
    if (arg == "--temperature") {
      const char* value = require_value(arg);
      if (value == nullptr) {
        return 2;
      }
      std::optional<double> parsed = parse_double(value);
      if (!parsed) {
        std::cerr << "--temperature must be a number\n";
        return 2;
      }
      options.temperature = *parsed;
      continue;
    }
    if (arg == "--top-p") {
      const char* value = require_value(arg);
      if (value == nullptr) {
        return 2;
      }
      std::optional<double> parsed = parse_double(value);
      if (!parsed) {
        std::cerr << "--top-p must be a number\n";
        return 2;
      }
      options.top_p = *parsed;
      continue;
    }

    std::cerr << "unknown argument: " << arg << '\n';
    print_usage(std::cerr);
    return 2;
  }

  if (config.model_path.empty() || config.context_tokens <= 0 ||
      config.threads <= 0 || options.max_tokens <= 0 ||
      options.temperature < 0.0 || options.top_p <= 0.0 || options.top_p > 1.0) {
    print_usage(std::cerr);
    return 2;
  }

  return yllama::run_stdio(std::cin, std::cout, std::cerr, config, options);
}
