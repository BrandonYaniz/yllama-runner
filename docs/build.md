# Build

`yllama-runner` is a C++ runner around llama.cpp.

## Requirements

- C++17 or newer compiler
- CMake

The default build does not require llama.cpp. The llama.cpp backend is enabled only when explicit local dependency paths are provided.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

This path is currently verified on macOS with Apple clang. FreeBSD and Linux verification are planned before the beta build.

## llama.cpp backend options

The project does not download llama.cpp during configuration. Build or install llama.cpp separately, then pass explicit paths:

```sh
cmake -S . -B build-llama \
  -DYLLAMA_ENABLE_LLAMA_BACKEND=ON \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp/include \
  -DYLLAMA_LLAMA_CPP_LIBRARIES=/path/to/libllama.a
```

`YLLAMA_LLAMA_CPP_LIBRARIES` accepts a semicolon-separated list when the local llama.cpp build needs more than one library.

For local testing, `deps/` is ignored by git and can hold a temporary llama.cpp checkout or build artifacts. That directory is not part of the package.

When enabled, the llama.cpp backend loads the configured GGUF model during the `configure` command. Token streaming is implemented in a later milestone.

## Install

```sh
cmake --install build
```

## Runtime behavior

The runner does not listen on any network port.

It uses:

- stdin for commands
- stdout for protocol events
- stderr for logs
