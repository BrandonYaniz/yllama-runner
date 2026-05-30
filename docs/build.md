# Build

`yllama-runner` is a C++ runner around llama.cpp.

## Requirements

- C++17 or newer compiler
- CMake
- A local llama.cpp build or install

The project does not download llama.cpp during configuration. Build or install llama.cpp separately, then pass explicit paths.

## Build

```sh
cmake -S . -B build \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp/include \
  -DYLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS=/path/to/llama.cpp/ggml/include \
  -DYLLAMA_LLAMA_CPP_LIBRARIES=/path/to/libllama.a
cmake --build build
ctest --test-dir build
```

This path is currently verified on macOS with Apple clang. FreeBSD and Linux verification are planned before the beta build.

## llama.cpp options

`YLLAMA_LLAMA_CPP_LIBRARIES` accepts a semicolon-separated list when the local llama.cpp build needs more than one library.

For local testing, `deps/` is ignored by git and can hold a temporary llama.cpp checkout or build artifacts. That directory is not part of the package.

The runner loads the configured GGUF model during the `configure` command and streams generated token deltas during `generate`.

An opt-in smoke test can run the built runner against a local GGUF model:

```sh
cmake -S . -B build-llama \
  -DYLLAMA_ENABLE_LLAMA_SMOKE_TEST=ON \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp/include \
  -DYLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS=/path/to/llama.cpp/ggml/include \
  -DYLLAMA_LLAMA_CPP_LIBRARIES='/path/to/libllama.dylib;/path/to/libggml.dylib' \
  -DYLLAMA_LLAMA_SMOKE_MODEL_PATH=/path/to/model.gguf \
  -DYLLAMA_LLAMA_SMOKE_LIBRARY_PATH=/path/to/runtime/libs
cmake --build build-llama
ctest --test-dir build-llama --output-on-failure
```

The smoke test is disabled by default because it requires a local model and a matching llama.cpp build.

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
