#include <iostream>
#include <string_view>

#include "runner.hpp"

namespace {

#ifndef YLLAMA_RELEASE_VERSION
#define YLLAMA_RELEASE_VERSION "0.0.0-local"
#endif

constexpr std::string_view kVersion = YLLAMA_RELEASE_VERSION;

void print_usage(std::ostream& out) {
  out << "usage: yllama-runner [--version]\n";
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

  if (argc > 1) {
    print_usage(std::cerr);
    return 2;
  }

  return yllama::run_stdio(std::cin, std::cout, std::cerr);
}
