# yllama-runner

`yllama-runner` is a small llama.cpp-based local inference runner for developers who want local GGUF inference behind a simple process boundary.

It is not a daemon and does not listen on a socket. It reads JSON Lines from stdin, writes JSON Lines to stdout, and writes logs to stderr.

## Why use it

Many local model integrations start by embedding inference code directly into an application or by standing up an HTTP server. Both approaches add coupling: applications inherit model lifecycle details, and local services introduce ports, routing, authentication, and deployment behavior that may not be needed.

`yllama-runner` keeps that boundary small. A parent process starts the runner, sends JSON Lines commands over stdin, and reads JSON Lines events from stdout. That gives developers a predictable way to load a local model, stream tokens, cancel work, and shut down cleanly without adding a network service.

This is useful for:

- Desktop apps that want private local inference without opening a port.
- CLI tools that need streamed model output from a child process.
- Test harnesses that need a stable JSON protocol around local generation.
- Applications that want to keep model execution isolated from the main process.

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

The CMake build and test path is currently verified on macOS with Apple clang.

Install:

```sh
cmake --install build
```

The real llama.cpp backend is not enabled yet. The current target is a build skeleton for the runner executable and test setup.

## Current status

The runner can print its version, emit the startup `hello` event, parse the initial command set, and run the stdio command loop through a backend interface. The current backend is fake; model loading and real generation are still in progress.

## License

BSD 3-Clause License.
