#include "runner.hpp"

#include <istream>
#include <ostream>
#include <string>

#include "jsonl.hpp"
#include "protocol.hpp"

namespace yllama {
namespace {

struct RuntimeState {
  bool configured = false;
  std::string model_path;
  int context_tokens = 0;
};

void emit(std::ostream& out, std::string_view event_json) {
  write_json_line(out, event_json);
  out.flush();
}

void handle_configure(const ConfigureCommand& command,
                      RuntimeState& state,
                      std::ostream& out) {
  state.configured = true;
  state.model_path = command.model_path;
  state.context_tokens = command.context_tokens;

  emit(out, ready_event(command.id, state.model_path, state.context_tokens));
}

void handle_generate(const GenerateCommand& command,
                     const RuntimeState& state,
                     std::ostream& out) {
  if (!state.configured) {
    emit(out, error_event(command.id, "not_configured",
                          "Runner must be configured before generation."));
    return;
  }

  emit(out, started_event(command.id));
  emit(out, delta_event(command.id, "fake response"));
  emit(out, completed_event(command.id, "stop", Usage{0, 2}));
}

void handle_cancel(const CancelCommand& command, std::ostream& out) {
  emit(out, error_event(command.id, "request_not_active",
                        "No active request matched the cancel command."));
}

}  // namespace

int run_stdio(std::istream& in, std::ostream& out, std::ostream& err) {
  RuntimeState state;

  emit(out, hello_event());

  std::string line;
  while (std::getline(in, line)) {
    ParseResult parsed = parse_command_line(line);
    if (parsed.error) {
      emit(out, error_event("", parsed.error->code, parsed.error->message));
      continue;
    }

    const Command& command = *parsed.command;
    if (const auto* configure = std::get_if<ConfigureCommand>(&command)) {
      handle_configure(*configure, state, out);
      continue;
    }
    if (const auto* generate = std::get_if<GenerateCommand>(&command)) {
      handle_generate(*generate, state, out);
      continue;
    }
    if (const auto* cancel = std::get_if<CancelCommand>(&command)) {
      handle_cancel(*cancel, out);
      continue;
    }
    if (std::get_if<ShutdownCommand>(&command) != nullptr) {
      return 0;
    }
  }

  if (!in.eof()) {
    err << "failed while reading stdin\n";
    return 1;
  }

  return 0;
}

}  // namespace yllama
