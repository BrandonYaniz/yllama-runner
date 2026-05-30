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

The llama.cpp backend is disabled by default. To configure a build against an existing local llama.cpp build, pass explicit paths:

```sh
cmake -S . -B build-llama \
  -DYLLAMA_ENABLE_LLAMA_BACKEND=ON \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp/include \
  -DYLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS=/path/to/llama.cpp/ggml/include \
  -DYLLAMA_LLAMA_CPP_LIBRARIES=/path/to/libllama.a
```

Install:

```sh
cmake --install build
```

The llama.cpp backend can be built from explicit local paths and currently handles model loading during `configure`. Token generation still uses the fake backend unless llama support is enabled, and real llama.cpp token streaming is still in progress.

## Current status

The runner can print its version, emit the startup `hello` event, parse the initial command set, render generation input, and run the stdio command loop through a backend interface. The default backend is fake; the opt-in llama.cpp backend can load a configured model, while real token generation is still in progress.

## License

BSD 3-Clause License.
