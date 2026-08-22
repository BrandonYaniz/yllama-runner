# Security Policy

## Supported Versions

`yllama-runner` uses calendar versioning for public releases.

Security fixes are generally provided for the latest release.

| Version        | Supported                    |
| -------------- | ---------------------------- |
| Latest release | Yes                          |
| Older releases | No, unless explicitly stated |

Because this project is small and early in development, users should assume that older releases are not maintained unless a release note says otherwise.

## Reporting a Vulnerability

Please report security vulnerabilities privately.

Contact:

```text
brandon@yaniz.io
```

Please include:

* A clear description of the issue.
* Steps to reproduce the issue.
* The affected version or commit.
* Any relevant logs, input examples, or crash details.
* Whether the issue is public or has already been disclosed elsewhere.

Do not open a public GitHub issue for a suspected security vulnerability unless the issue has already been coordinated or is not sensitive.

## Response Expectations

This is a small open source project maintained by its owner. Security reports will be reviewed as time allows.

The maintainer may:

* Confirm the issue.
* Ask for more information.
* Reject the report if it is not a security issue.
* Prepare a fix.
* Publish a security advisory or release note when appropriate.

No specific response timeline is guaranteed.

## Project Security Model

`yllama-runner` is designed to be a local child process runner for llama.cpp.

The runner:

* Reads bounded binary messages from stdin.
* Writes bounded binary events to stdout.
* Writes logs and diagnostics to stderr.
* Does not expose an HTTP API.
* Does not listen on TCP, UDP, Unix sockets, or other network interfaces.
* Does not download models.
* Does not provide authentication, authorization, accounts, or multi-user access control.
* Does not sandbox model execution.
* Does not attempt to make untrusted GGUF files safe.

A parent process is responsible for deciding when to start the runner, what model file to load, what prompts to send, and how to handle output.

## Security Boundaries

The stdio protocol is the primary boundary between the parent process and `yllama-runner`.

Expected boundaries:

* stdin is for bounded binary Generate and Cancel messages.
* stdout is for bounded binary Ready, Chunk, Completed, and Error events.
* stderr is for human-readable logs and diagnostics.
* Logs must not be written to stdout because stdout is part of the parent-facing output channel.
* Parent processes should wait for Ready before sending work.
* Parent processes should enforce startup, request, and cancellation timeouts.

## Out of Scope

The following are outside the security scope of this project:

* Malicious model files.
* Malicious llama.cpp builds.
* Compromised local systems.
* Prompt injection against applications that consume model output.
* Network exposure added by a parent application.
* Authentication and authorization in parent applications.
* Sandboxing, containerization, jail configuration, or operating system hardening.
* Supply chain issues in dependencies unless directly caused by this project.

Users who run untrusted models, untrusted prompts, or untrusted parent applications should provide their own isolation.

## Sensitive Data

Prompts, generated text, file paths, and diagnostic logs may contain sensitive information.

Users and parent applications should avoid sending secrets unless they are prepared for those secrets to be processed by the local model stack.

Do not assume that model output is safe, private, correct, or free from sensitive data.

## Dependency Security

`yllama-runner` depends on a local llama.cpp build supplied by the developer or packager.

Users and packagers should:

* Build llama.cpp from trusted sources.
* Keep llama.cpp updated.
* Track security issues in compiler, CMake, libc, and platform dependencies.
* Avoid mixing untrusted model files with privileged execution contexts.

## Safe Deployment Guidance

Recommended usage:

* Run `yllama-runner` as an unprivileged user.
* Do not run the runner as root.
* Do not expose the runner directly to a network.
* Keep network access, authentication, and authorization in the parent application if needed.
* Treat model files as untrusted input unless they come from a trusted source.
* Keep machine-readable protocol output on stdout separate from diagnostics on stderr.
* Use operating system controls, jails, containers, or sandboxes when stronger isolation is required.

## Disclosure

If a vulnerability is confirmed, the maintainer may publish:

* A patched release.
* A GitHub security advisory.
* A release note describing the issue.
* Mitigation guidance.

Public disclosure should avoid unnecessary exploit detail until users have had a reasonable chance to update.
