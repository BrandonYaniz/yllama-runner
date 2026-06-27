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

## Manual Framed Generate Check

Run one framed request through the installed runner:

```sh
RUNNER="$PREFIX/libexec/yllama-runner"

python3 - "$RUNNER" "$MODEL" <<'PY'
import struct
import subprocess
import sys

runner, model = sys.argv[1], sys.argv[2]
prompt = b"Complete this sentence in four words: The sky is"
proc = subprocess.Popen(
    [runner, "--model", model, "--ctx", "1024", "--threads", "4",
     "--max-tokens", "8", "--temperature", "0"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)
proc.stdin.write(struct.pack("<I", len(prompt)) + prompt)
proc.stdin.close()

while True:
    tag = proc.stdout.read(1)
    if tag == b"\x01":
        size = struct.unpack("<I", proc.stdout.read(4))[0]
        sys.stdout.buffer.write(proc.stdout.read(size))
    elif tag == b"\x02":
        break
    elif tag == b"\x03":
        size = struct.unpack("<H", proc.stdout.read(2))[0]
        raise SystemExit(proc.stdout.read(size).decode())
    else:
        raise SystemExit(f"unexpected frame tag: {tag!r}")

status = proc.wait()
if status:
    raise SystemExit(proc.stderr.read().decode())
PY
```

Expected stdout is generated model text. stderr is diagnostics only.

## Cancellation Check

There is no in-band cancel command. Parent processes cancel by terminating the
runner process and starting a fresh resident runner when needed.

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
