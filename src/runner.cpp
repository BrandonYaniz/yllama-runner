#include "runner.hpp"

#include <istream>
#include <memory>
#include <ostream>

#include "backend.hpp"
#include "frame.hpp"

namespace yllama {
namespace {

std::string backend_error_message(const BackendError& error) {
  if (error.code.empty()) {
    return error.message;
  }
  if (error.message.empty()) {
    return error.code;
  }
  return error.code + ": " + error.message;
}

bool emit_error(std::ostream& out, std::string_view message) {
  const bool ok = write_error_frame(out, message);
  out.flush();
  return ok;
}

}  // namespace

int run_stdio(std::istream& in,
              std::ostream& out,
              std::ostream& err,
              const RunnerConfig& config,
              const GenerateOptions& options) {
  std::unique_ptr<Backend> backend = make_default_backend();
  return run_stdio(in, out, err, config, options, *backend);
}

int run_stdio(std::istream& in,
              std::ostream& out,
              std::ostream& err,
              const RunnerConfig& config,
              const GenerateOptions& options,
              Backend& backend) {
  ConfigureResult configured = backend.configure(config);
  if (configured.error) {
    err << backend_error_message(*configured.error) << '\n';
    return 1;
  }

  while (true) {
    PromptFrame frame = read_prompt_frame(in);
    if (frame.status == ReadFrameStatus::Eof) {
      return 0;
    }
    if (frame.status == ReadFrameStatus::Invalid) {
      err << frame.error << '\n';
      return 1;
    }

    bool write_failed = false;
    GenerateResult result = backend.generate(
        frame.prompt, options,
        [&](std::string_view text) {
          if (!text.empty() && !write_failed) {
            write_failed = !write_chunk_frame(out, text);
            out.flush();
          }
        },
        []() { return false; });

    if (write_failed) {
      err << "failed to write chunk frame\n";
      return 1;
    }

    if (result.error) {
      if (!emit_error(out, backend_error_message(*result.error))) {
        err << "failed to write error frame\n";
        return 1;
      }
      continue;
    }

    if (!write_done_frame(out)) {
      err << "failed to write done frame\n";
      return 1;
    }
    out.flush();
  }
}

}  // namespace yllama
