# Build and package

Requirements are CMake 3.16+, a C++17 compiler, Git, and a local GGUF only for
smoke testing. The production build fetches the immutable llama.cpp commit in
`CMakeLists.txt`:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix staging
```

On macOS arm64 llama.cpp enables Metal and Accelerate when available. Linux
amd64/arm64 and FreeBSD amd64 use CPU backends. Release builds are static by
default. When shared libraries are explicitly enabled, installed lookup is
relative via `@loader_path` on macOS and `$ORIGIN` on ELF. Release artifacts
must pass `install-relocation`, which copies the executable to a fresh temporary
directory and rejects the original build directory in runtime paths.

For an opt-in real model test:

```sh
cmake -S . -B build-release \
  -DYLLAMA_ENABLE_LLAMA_SMOKE_TEST=ON \
  -DYLLAMA_LLAMA_SMOKE_MODEL_PATH=/path/to/small-model.gguf
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

The smoke test loads the model, validates Ready, performs two requests in the
same process, checks UTF-8 and usage metadata, and sends Shutdown. The local
development override variables `YLLAMA_LLAMA_CPP_INCLUDE_DIR`,
`YLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS`, and `YLLAMA_LLAMA_CPP_LIBRARIES` remain
available, but external shared-library builds are not release artifacts.

The default `--gpu-layers` is 0. `--build-info` identifies compiled backends and
the pinned dependency. llama.cpp logs are suppressed unless configured with
`-DYLLAMA_ENABLE_LLAMA_LOGS=ON`.
