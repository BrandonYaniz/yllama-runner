# Build

`yllama-runner` is a C++ runner around llama.cpp.

## Requirements

- C++17 or newer compiler
- CMake
- llama.cpp source or linked library
- nlohmann/json or compatible JSON library

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Install

```sh
install -m 0755 build/yllama-runner /usr/local/libexec/yllama-runner
```

## Runtime behavior

The runner does not listen on any network port.

It uses:

- stdin for commands
- stdout for protocol events
- stderr for logs
