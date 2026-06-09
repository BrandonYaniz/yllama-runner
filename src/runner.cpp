#include "runner.hpp"

#include <atomic>
#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>

#include "backend.hpp"
#include "jsonl.hpp"
#include "protocol.hpp"

namespace yllama {
namespace {

struct RuntimeState {
  bool configured = false;
  std::string model_path;
  int context_tokens = 0;
};

struct ActiveRequest {
  std::string id;
  std::atomic<bool> cancel_requested{false};
  std::atomic<bool> done{false};
  std::thread worker;
};

void emit(std::ostream& out, std::string_view event_json) {
  write_json_line(out, event_json);
  out.flush();
}

void emit_locked(std::ostream& out,
                 std::mutex& out_mutex,
                 std::string_view event_json) {
  std::lock_guard<std::mutex> lock(out_mutex);
  emit(out, event_json);
}

void join_active_request(std::unique_ptr<ActiveRequest>& active) {
  if (active && active->worker.joinable()) {
    active->worker.join();
  }
  active.reset();
}

void reap_finished_request(std::unique_ptr<ActiveRequest>& active) {
  if (active && active->done.load()) {
    join_active_request(active);
  }
}

void handle_configure(const ConfigureCommand& command,
                      RuntimeState& state,
                      Backend& backend,
                      std::ostream& out,
                      std::mutex& out_mutex) {
  ConfigureResult result = backend.configure(command);
  if (result.error) {
    emit_locked(out, out_mutex,
                error_event(command.id, result.error->code, result.error->message));
    return;
  }

  state.configured = true;
  state.model_path = result.model_path;
  state.context_tokens = result.context_tokens;

  emit_locked(out, out_mutex,
              ready_event(command.id, state.model_path, state.context_tokens));
}

void handle_generate(const GenerateCommand& command,
                     const RuntimeState& state,
                     Backend& backend,
                     std::ostream& out,
                     std::mutex& out_mutex,
                     std::unique_ptr<ActiveRequest>& active) {
  if (!state.configured) {
    emit_locked(out, out_mutex,
                error_event(command.id, "not_configured",
                            "Runner must be configured before generation."));
    return;
  }

  if (active) {
    emit_locked(out, out_mutex,
                error_event(command.id, "request_active",
                            "Runner already has an active generation request."));
    return;
  }

  active = std::make_unique<ActiveRequest>();
  active->id = command.id;
  ActiveRequest* request = active.get();
  GenerateCommand request_command = command;

  request->worker =
      std::thread([request_command, request, &backend, &out, &out_mutex]() {
        emit_locked(out, out_mutex, started_event(request_command.id));

        const bool stream = request_command.settings.stream.value_or(true);
        std::string buffered_text;
        GenerateResult result = backend.generate(
            request_command,
            [&](std::string_view text) {
              if (stream) {
                emit_locked(out, out_mutex, delta_event(request_command.id, text));
              } else {
                buffered_text.append(text);
              }
            },
            [request]() { return request->cancel_requested.load(); });

        if (result.error) {
          emit_locked(out, out_mutex,
                      error_event(request_command.id, result.error->code,
                                  result.error->message));
        } else if (result.finish_reason == "cancelled") {
          emit_locked(out, out_mutex, cancelled_event(request_command.id));
        } else if (stream) {
          emit_locked(out, out_mutex,
                      completed_event(request_command.id, result.finish_reason,
                                      result.usage));
        } else {
          emit_locked(out, out_mutex,
                      completed_event(request_command.id, result.finish_reason,
                                      result.usage, buffered_text));
        }

        request->done.store(true);
      });
}

void handle_cancel(const CancelCommand& command,
                   std::ostream& out,
                   std::mutex& out_mutex,
                   const std::unique_ptr<ActiveRequest>& active) {
  if (!active || active->id != command.id || active->done.load()) {
    emit_locked(out, out_mutex,
                error_event(command.id, "request_not_active",
                            "No active request matched the cancel command."));
    return;
  }

  active->cancel_requested.store(true);
}

}  // namespace

int run_stdio(std::istream& in, std::ostream& out, std::ostream& err) {
  std::unique_ptr<Backend> backend = make_default_backend();
  return run_stdio(in, out, err, *backend);
}

int run_stdio(std::istream& in,
              std::ostream& out,
              std::ostream& err,
              Backend& backend) {
  RuntimeState state;
  std::mutex out_mutex;
  std::unique_ptr<ActiveRequest> active;

  emit_locked(out, out_mutex, hello_event());

  std::string line;
  while (std::getline(in, line)) {
    reap_finished_request(active);

    ParseResult parsed = parse_command_line(line);
    if (parsed.error) {
      emit_locked(out, out_mutex,
                  error_event("", parsed.error->code, parsed.error->message));
      continue;
    }

    const Command& command = *parsed.command;
    if (const auto* configure = std::get_if<ConfigureCommand>(&command)) {
      if (active) {
        emit_locked(out, out_mutex,
                    error_event(configure->id, "request_active",
                                "Runner already has an active generation request."));
      } else {
        handle_configure(*configure, state, backend, out, out_mutex);
      }
      continue;
    }
    if (const auto* generate = std::get_if<GenerateCommand>(&command)) {
      handle_generate(*generate, state, backend, out, out_mutex, active);
      continue;
    }
    if (const auto* cancel = std::get_if<CancelCommand>(&command)) {
      handle_cancel(*cancel, out, out_mutex, active);
      continue;
    }
    if (std::get_if<ShutdownCommand>(&command) != nullptr) {
      join_active_request(active);
      return 0;
    }
  }

  if (active) {
    active->cancel_requested.store(true);
    join_active_request(active);
  }

  if (!in.eof()) {
    err << "failed while reading stdin\n";
    return 1;
  }

  return 0;
}

}  // namespace yllama
