# Versioning and compatibility

Runner catalog releases use `YY.MM.DD.NN` or `YY.MM.DD.NN-Release`. The final
two digits are a same-day sequence. The current release is
`26.07.16.01-Release`.

Wire protocol versions are independent unsigned integers and are never inferred
from the release string. This release supports protocols 1 and 2. Protocol 1 is
the compatibility default; protocol 2 is selected with `--protocol 2`. See
[the protocol 2 contract](yllmd-protocol-v2-migration.md) for the deprecation
policy.

`yllama-runner --version` prints the catalog release. `--build-info` also prints
supported protocols, the llama.cpp commit, backends, OS, and architecture.
The repository-root `VERSION` file is authoritative; CMake reads it as a normal
source input rather than a cache-overridable release value. Release and CI jobs
must configure in a clean build directory, and the release contract test checks
the source file, executable outputs, this document, and CI expectation agree.
