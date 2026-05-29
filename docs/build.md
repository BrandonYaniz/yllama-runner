# Build

`yllama-runner` is a C++ runner around llama.cpp.

## Requirements

- C++17 or newer compiler
- CMake

The initial build skeleton does not require llama.cpp or nlohmann/json yet. Those dependencies will be introduced behind explicit CMake options as the backend and protocol layers are added.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

This path is currently verified on macOS with Apple clang. FreeBSD and Linux verification are planned before the beta build.

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
