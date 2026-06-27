# Versioning and Compatibility

`yllama-runner` uses calendar versioning for releases and a separate protocol version for compatibility.

This keeps the public release number simple while still making compatibility explicit for parent processes that communicate with the runner.

## Release Versions

Public releases use this format:

```text
YYYY.MM.DD
```

Prereleases use this format:

```text
YYYY.MM.DD-alpha.N
YYYY.MM.DD-beta.N
YYYY.MM.DD-rc.N
```

Examples:

```text
2026.05.31-alpha.1
2026.05.31-beta.1
2026.05.31-rc.1
2026.05.31
```

Git tags use a leading `v`:

```text
v2026.05.31
v2026.05.31-rc.1
```

The release version identifies the date of the release. It does not, by itself, describe whether the stdio protocol changed.

## Compatibility Versions

`yllama-runner` has one primary compatibility contract: the stdio protocol used between the parent process and the runner.

The runner:

- Reads JSON Lines commands from stdin.
- Writes JSON Lines events or requested raw generated text to stdout.
- Writes logs and diagnostics to stderr.

Because the runner does not expose an HTTP API, socket API, persistent database schema, plugin interface, or user-facing daemon configuration, the only compatibility version currently tracked is the runner protocol version.

Current protocol version:

```text
1
```

## Startup Hello Event

The runner exposes the protocol version in its startup `hello` event.

Example:

```json
{"type":"hello","protocol_version":1,"runner":"yllama-runner","capabilities":["generate","stream","cancel","output_modes"]}
```

Parent processes should read the `hello` event before sending generation commands. They should verify the protocol version and check capabilities instead of assuming behavior from the release date.

The release version is available from:

```sh
yllama-runner --version
```

A future compatible protocol update may also include the release version in `hello` as an optional field.

## Compatible Changes

Compatible changes do not require a protocol version bump.

Examples include:

- Adding optional fields to events.
- Adding optional fields to commands.
- Adding a new capability flag.
- Adding a new event type that older callers can safely ignore.
- Improving logs written to stderr.
- Fixing behavior so it matches the documented protocol.
- Improving llama.cpp integration without changing command or event semantics.
- Improving build logic, packaging, tests, or documentation.

## Breaking Changes

Breaking changes require a protocol version bump.

Examples include:

- Renaming a required command field.
- Removing a command.
- Removing a required event field.
- Renaming an event type.
- Changing the meaning of an existing event type.
- Changing token streaming behavior in a way callers must handle differently.
- Changing cancellation behavior in a way callers must handle differently.
- Changing request identifiers, response correlation, or error structure in a non-compatible way.
- Moving machine-readable protocol output from stdout to stderr.
- Moving logs or diagnostics from stderr to stdout.
- Requiring parent processes to send a new field that older callers do not send.

## Deprecation Policy

When possible, compatible deprecations should be used before breaking changes.

A deprecation should include:

- What is deprecated.
- What should be used instead.
- The earliest release date when removal may happen.

Example:

```markdown
The `prompt` field is deprecated as of `2026.07.01`.
Use `messages` instead.
Removal is planned no earlier than `2026.10.01`.
```

Security, correctness, or data-safety issues may require faster removal.

## Release Notes Format

Each release should include a compatibility section.

For a normal compatible release:

```markdown
## Compatibility

- Release version: 2026.05.31
- Runner protocol version: 1

## Breaking Changes

- None.

## Deprecations

- None.
```

For a breaking release:

```markdown
## Compatibility

- Release version: 2026.08.15
- Runner protocol version: 2

## Breaking Changes

- Runner protocol changed from 1 to 2.
- The `generate` command now requires `messages` instead of `prompt`.

## Migration Notes

- Replace `prompt` with a single user message in `messages`.
```

## Summary

The release version answers:

```text
When was this released?
```

The protocol version answers:

```text
Can my parent process safely talk to this runner?
```
