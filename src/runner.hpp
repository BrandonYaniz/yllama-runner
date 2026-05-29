#ifndef YLLAMA_RUNNER_RUNNER_HPP
#define YLLAMA_RUNNER_RUNNER_HPP

#include <iosfwd>

namespace yllama {

class Backend;

int run_stdio(std::istream& in, std::ostream& out, std::ostream& err);
int run_stdio(std::istream& in,
              std::ostream& out,
              std::ostream& err,
              Backend& backend);

}  // namespace yllama

#endif
