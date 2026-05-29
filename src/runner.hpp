#ifndef YLLAMA_RUNNER_RUNNER_HPP
#define YLLAMA_RUNNER_RUNNER_HPP

#include <iosfwd>

namespace yllama {

int run_stdio(std::istream& in, std::ostream& out, std::ostream& err);

}  // namespace yllama

#endif
