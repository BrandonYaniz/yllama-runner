#ifndef YLLAMA_RUNNER_RUNNER_HPP
#define YLLAMA_RUNNER_RUNNER_HPP

#include <iosfwd>

namespace yllama {

class Backend;
struct RunnerConfig;

int run_stdio(std::istream& in,
              std::ostream& out,
              std::ostream& err,
              const RunnerConfig& config);
int run_stdio(std::istream& in,
              std::ostream& out,
              std::ostream& err,
              const RunnerConfig& config,
              Backend& backend);

}  // namespace yllama

#endif
