# Versioning and Compatibility

`yllama-runner` uses calendar versioning for releases.

Public releases use:

```text
YYYY.MM.DD
```

Prereleases use:

```text
YYYY.MM.DD-alpha.N
YYYY.MM.DD-beta.N
YYYY.MM.DD-rc.N
```

The release version is available from:

```sh
yllama-runner --version
```

## Compatibility

The runner is pre-release and has one internal compatibility contract: the
binary stdio frame protocol used by a parent process such as `yllmd`.

The runner:

- reads length-prefixed prompt frames from stdin
- writes chunk, done, or error frames to stdout
- writes diagnostics to stderr

There is no separate protocol negotiation or startup hello event. During
pre-release development, breaking transport changes may be made directly across
`yllama-runner` and `yllmd`.

## Release Notes

Each release should call out whether the runner frame contract changed.
