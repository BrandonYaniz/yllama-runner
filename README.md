# yllama-runner

`yllama-runner` is a small llama.cpp-based local inference runner for `yllmd`.

It is not a daemon and does not listen on a socket. It reads JSON Lines from stdin, writes JSON Lines to stdout, and writes logs to stderr.

## Goals

- Load one local GGUF model.
- Accept generation requests over stdio.
- Stream token deltas as JSON Lines.
- Support cancellation.
- Exit cleanly when requested.
- Avoid HTTP, HTTPS, TCP, and public IPC.

## Non-goals

- No HTTP server.
- No Unix socket listener.
- No remote providers.
- No model downloads.
- No model update logic.
- No request queueing.
- No user-facing daemon behavior.

## Relationship to yllmd

`yllmd` starts and supervises `yllama-runner`. The daemon owns client access, queueing, routing, model download, model update, and lifecycle policy.

`yllama-runner` only owns local model execution.

## Build

Requirements:

- CMake 3.16 or newer
- C++17 compiler

Build and run the initial smoke test:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Install:

```sh
cmake --install build
```

The real llama.cpp backend is not enabled yet. The current target is a build skeleton for the runner executable and test setup.

## Current status

The runner can print its version, emit the startup `hello` event, parse the initial command set, and run the stdio command loop with a fake generation response. Model loading and real generation are still in progress.

## License

BSD 3-Clause License.
