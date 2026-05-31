# yllama-runner Protocol

`yllama-runner` communicates over JSON Lines.

The runner is intended to be started by a parent process. It does not listen on a network socket and does not behave as a user-facing daemon.

## Streams

```text
stdin   JSON Lines commands from the parent process
stdout  JSON Lines events from the runner
stderr  human-readable logs and diagnostics
```

stdout is part of the machine-readable protocol. Logs should not be written to stdout.

stderr is not part of the machine-readable protocol. Parent processes may capture it for diagnostics, but should not depend on it for normal protocol behavior.

## Protocol Version

Current protocol version:

```text
1
```

The protocol version changes only when the stdin/stdout JSON Lines contract changes in a way that may break parent processes.

## Hello Event

The runner should emit a startup `hello` event before accepting generation work.

Example:

```json
{"type":"hello","protocol_version":1,"runner":"yllama-runner","capabilities":["generate","stream","cancel"]}
```

Parent processes should verify `protocol_version` and inspect `capabilities`.

## Capability Flags

Capability flags describe optional or behaviorally important features.

Current capabilities:

```json
["generate","stream","cancel"]
```

Callers should treat unknown capabilities as informational and should not fail just because a new capability is present.

## Compatibility Rules

The protocol should remain backward-compatible within the same protocol version.

Compatible changes include:

- Adding optional fields.
- Adding new capability flags.
- Adding event types that older callers can ignore.
- Improving errors without changing required error fields.
- Fixing behavior to match this document.

Breaking changes require a new protocol version.

Breaking changes include:

- Removing or renaming required fields.
- Removing commands.
- Renaming event types.
- Changing event meanings.
- Changing request and response correlation.
- Changing token streaming semantics in a non-compatible way.
- Changing cancellation semantics in a non-compatible way.
- Requiring new fields that older callers do not send.

## Parent Process Guidance

A parent process should:

1. Start the runner.
2. Read the startup `hello` event.
3. Verify that the reported protocol version is supported.
4. Check required capabilities.
5. Send commands over stdin.
6. Read events from stdout.
7. Treat stderr as diagnostics only.

If the protocol version is not supported, the parent process should stop using the runner and report a clear compatibility error.
