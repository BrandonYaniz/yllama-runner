# yllama-runner

`yllama-runner` is a small llama.cpp-based local inference worker for local GGUF models.

It is not a daemon and does not listen on a socket. It loads one model from startup flags, reads binary prompt frames from stdin, streams binary output frames to stdout, and writes diagnostics to stderr.

## Why use it

Many local model integrations start by embedding inference code directly into an application or by standing up an HTTP server. Both approaches add coupling: applications inherit model lifecycle details, and local services introduce ports, routing, authentication, and deployment behavior that may not be needed.

`yllama-runner` keeps that boundary small. A parent process starts the runner with model configuration, sends prompt frames over stdin, and reads streamed generated text frames from stdout. Higher-level concerns such as request ids, queueing, structured client errors, and model routing belong in the parent process.

This is useful for:

- Desktop apps that want private local inference without opening a port.
- CLI tools that need streamed model output from a child process.
- Supervisors such as `yllmd` that provide the client-facing protocol.
- Applications that want to keep model execution isolated from the main process.

## Goals

- Load one local GGUF model.
- Accept bounded binary messages over stdio.
- Stream generated text chunks as binary frames.
- Keep the model resident across serial requests.
- Exit cleanly on stdin EOF.
- Avoid HTTP, HTTPS, TCP, and public IPC.

## Non-goals

- No HTTP server.
- No Unix socket listener.
- No remote providers.
- No model downloads.
- No model update logic.
- No request queueing.
- No JSON protocol.
- No request ids.
- No user-facing daemon behavior.

## Build

Requirements:

- CMake 3.16 or newer
- C++17 compiler
- A local GGUF model for runtime use

The default build fetches the immutable llama.cpp revision recorded in
`CMakeLists.txt`, then builds it with the runner:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

The CMake build and test path is currently verified on macOS with Apple clang.

For the current beta verification flow, see [docs/beta-testing.md](docs/beta-testing.md).

Install:

```sh
cmake --install build
```

The runner uses llama.cpp as its production backend. Tests still use an internal fake backend where that keeps frame and runner-loop checks deterministic.

## Current status

The runner loads one GGUF model, signals readiness, accepts sequential
per-request generation settings, supports cancellation and stop strings, and
streams valid UTF-8 with token counts and a completion reason.

Process flags are `--model`, `--ctx`, `--threads`, and `--gpu-layers` (default
0). See [the runner protocol](docs/runner-protocol.md). `--build-info` reports
the runner and dependency identity.

Current limitations:

- One model per runner process.
- Serial generation only.
- No request queueing.
- No network API.
- GGUF model files are supplied by the developer or packager.

## License

BSD 3-Clause License.
