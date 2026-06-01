# Beta Testing

This guide is for testers building `yllama-runner` from source with a local llama.cpp build and a local GGUF model.

The beta runner does not download llama.cpp or model files. Keep those dependencies outside the package, or under ignored local test directories such as `deps/`.

## Prerequisites

- CMake 3.16 or newer.
- A C++17 compiler.
- A built llama.cpp checkout or install.
- A small GGUF model suitable for smoke testing.

On macOS, the verified local test setup uses:

- AppleClang 21.0.0.
- llama.cpp built for CPU.
- `SmolLM2-135M-Instruct-Q4_K_M.gguf` as a lightweight test model.

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

Expected version:

```text
yllama-runner 2026.05.31-beta.1
```

## Manual Generate Check

Run a small prompt through the installed runner:

```sh
RUNNER="$PREFIX/libexec/yllama-runner"

printf '{"type":"configure","id":"cfg-001","model_path":"%s","context_tokens":1024,"threads":4}\n{"type":"generate","id":"req-generate","input":{"kind":"prompt","prompt":"Complete this sentence in four words: The sky is"},"settings":{"max_tokens":8,"temperature":0,"stop":[" blue"]}}\n{"type":"shutdown","id":"shutdown-001"}\n' "$MODEL" \
  | "$RUNNER" > /tmp/yllama-generate-out.jsonl 2> /tmp/yllama-generate-err.log

sed -n '1,20p' /tmp/yllama-generate-out.jsonl
```

Expected stdout events:

- `hello`
- `ready`
- `started`
- one or more `delta` events
- `completed`

stdout must contain only JSON Lines protocol events. llama.cpp diagnostics belong on stderr.

## Manual Cancel Check

Run a request and immediately cancel it:

```sh
RUNNER="$PREFIX/libexec/yllama-runner"

printf '{"type":"configure","id":"cfg-001","model_path":"%s","context_tokens":1024,"threads":4}\n{"type":"generate","id":"req-cancel","input":{"kind":"prompt","prompt":"Write a long paragraph about local inference."},"settings":{"max_tokens":128,"temperature":0}}\n{"type":"cancel","id":"req-cancel"}\n{"type":"shutdown","id":"shutdown-001"}\n' "$MODEL" \
  | "$RUNNER" > /tmp/yllama-cancel-out.jsonl 2> /tmp/yllama-cancel-err.log

sed -n '1,20p' /tmp/yllama-cancel-out.jsonl
```

Expected stdout events when cancellation is observed:

- `hello`
- `ready`
- `started`
- `cancelled`

Depending on hardware speed and model behavior, a short request may complete before cancellation is observed. If that happens, the terminal event may be `completed` instead of `cancelled`; stdout should still remain valid JSON Lines and stderr should contain only diagnostics.

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
- stdout JSONL output.
- stderr diagnostics.

Do not include private prompts, private model paths, or local secrets in reports.
