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
