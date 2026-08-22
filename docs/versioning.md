# Versioning and compatibility

Runner catalog releases use `YY.MM.DD.NN` or `YY.MM.DD.NN-Release`. The final
two digits are a same-day sequence. The current release is
`26.07.16.01-Release`.

`yllama-runner --version` prints the catalog release. `--build-info` also prints
the llama.cpp commit, backends, OS, and architecture. The internal stdio
contract is deliberately unversioned because `yllmd` and the runner are updated
together; incompatible binaries fail on framing or startup rather than
negotiating optional behavior.
The repository-root `VERSION` file is authoritative; CMake reads it as a normal
source input rather than a cache-overridable release value. Release and CI jobs
must configure in a clean build directory, and the release contract test checks
the source file, executable outputs, this document, and CI expectation agree.
