# Beta Testing

This guide is for testers building `yllama-runner` from source with a local
llama.cpp build and a local GGUF model.

## Prerequisites

- CMake 3.16 or newer.
- A C++17 compiler.
- A built llama.cpp checkout or install.
- A small GGUF model suitable for smoke testing.
- `python3` when enabling the opt-in llama smoke test.

## Build and Test

Set paths for your local environment:

```sh
LLAMA_ROOT="$PWD/deps/llama.cpp"
LLAMA_LIB_DIR="$LLAMA_ROOT/build-cpu/bin"
MODEL="$PWD/deps/models/SmolLM2-135M-Instruct-Q4_K_M.gguf"
PREFIX="/tmp/yllama-runner-beta"
```

Configure:

```sh
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
```

Build and run tests:

```sh
cmake --build build-beta-macos
ctest --test-dir build-beta-macos --output-on-failure
```

Install and check the installed binary:

```sh
cmake --install build-beta-macos
"$PREFIX/libexec/yllama-runner" --version
```

## Inference and cancellation checks

Configure with `YLLAMA_ENABLE_LLAMA_SMOKE_TEST=ON` as described in
[build.md](build.md), then run `ctest --test-dir build-beta-macos
--output-on-failure`. The smoke test verifies deterministic generation,
streaming UTF-8, cancellation, and a subsequent request in the same resident
process. The wire layout is documented in [runner-protocol.md](runner-protocol.md).

## Reporting Failures

Include:

- Operating system and version.
- Compiler and version.
- CMake version.
- llama.cpp commit or release.
- Whether llama.cpp was linked statically or dynamically.
- GGUF model name and quantization.
- Full CMake configure command.
- Failing command.
- stdout frame behavior.
- stderr diagnostics, if the build has llama.cpp logs enabled.

Do not include private prompts, private model paths, or local secrets in reports.
