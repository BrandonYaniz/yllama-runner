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
  -DYLLAMA_RELEASE_VERSION=2026.05.31-beta.1 \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp/include \
  -DYLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS=/path/to/llama.cpp/ggml/include \
  -DYLLAMA_LLAMA_CPP_LIBRARIES=/path/to/libllama.a
cmake --build build
ctest --test-dir build
```

This path is currently verified on macOS with Apple clang. FreeBSD and Linux verification are planned before the beta build.

## llama.cpp options

`YLLAMA_RELEASE_VERSION` sets the public version printed by `yllama-runner --version`. The default is the current beta release version.

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

By default, the runner installs to `${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/yllama-runner`. With the default GNU install layout, that is `libexec/yllama-runner` under the chosen prefix.

If the runner links shared llama.cpp libraries, those libraries must be discoverable at runtime:

- Use `CMAKE_INSTALL_RPATH` when building a package-style install.
- Use `DYLD_LIBRARY_PATH` for local macOS testing.
- Use `LD_LIBRARY_PATH` for local Linux or FreeBSD testing.
- Use static llama.cpp libraries when the package should avoid runtime library path setup.

Example with an install rpath:

```sh
cmake -S . -B build \
  -DYLLAMA_RELEASE_VERSION=2026.05.31-beta.1 \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp/include \
  -DYLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS=/path/to/llama.cpp/ggml/include \
  -DYLLAMA_LLAMA_CPP_LIBRARIES='/path/to/llama/lib/libllama.dylib;/path/to/llama/lib/libggml.dylib' \
  -DCMAKE_INSTALL_PREFIX=/tmp/yllama-runner-install \
  -DCMAKE_INSTALL_RPATH=/path/to/llama/lib
cmake --build build
cmake --install build
/tmp/yllama-runner-install/libexec/yllama-runner --version
```

## Runtime behavior

The runner does not listen on any network port.

It uses:

- stdin for commands
- stdout for protocol events
- stderr for logs

## macOS beta verification

The current macOS beta path uses a local llama.cpp build and a local lightweight GGUF model under ignored `deps/` content. Adjust paths if llama.cpp is installed elsewhere.

```sh
LLAMA_ROOT="$PWD/deps/llama.cpp"
LLAMA_LIB_DIR="$LLAMA_ROOT/build-cpu/bin"
MODEL="$PWD/deps/models/SmolLM2-135M-Instruct-Q4_K_M.gguf"
PREFIX="/tmp/yllama-runner-beta"

cmake -S . -B build-beta-macos \
  -DYLLAMA_RELEASE_VERSION=2026.05.31-beta.1 \
  -DYLLAMA_ENABLE_LLAMA_SMOKE_TEST=ON \
  -DYLLAMA_LLAMA_CPP_INCLUDE_DIR="$LLAMA_ROOT/include" \
  -DYLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS="$LLAMA_ROOT/ggml/include" \
  -DYLLAMA_LLAMA_CPP_LIBRARIES="$LLAMA_LIB_DIR/libllama.dylib;$LLAMA_LIB_DIR/libggml.dylib;$LLAMA_LIB_DIR/libggml-cpu.dylib;$LLAMA_LIB_DIR/libggml-blas.dylib;$LLAMA_LIB_DIR/libggml-base.dylib" \
  -DYLLAMA_LLAMA_SMOKE_MODEL_PATH="$MODEL" \
  -DYLLAMA_LLAMA_SMOKE_LIBRARY_PATH="$LLAMA_LIB_DIR" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_INSTALL_RPATH="$LLAMA_LIB_DIR"

cmake --build build-beta-macos
ctest --test-dir build-beta-macos --output-on-failure
cmake --install build-beta-macos
"$PREFIX/libexec/yllama-runner" --version
```
