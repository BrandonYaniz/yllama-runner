#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kVersion = "0.1.0";

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

  std::cerr << "runner protocol is not implemented yet\n";
  return 1;
}
