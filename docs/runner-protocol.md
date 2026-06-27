# Runner Protocol

`yllama-runner` is a resident single-model worker intended to be launched by
`yllmd`. It uses binary frames over stdio.

- stdin receives prompt frames.
- stdout emits generated text chunk frames and terminal frames.
- stderr is reserved for diagnostics.

The runner does not expose request ids, JSON, model routing, queueing,
cancellation commands, token usage, or output modes. Those belong in `yllmd`.

## Startup

Configuration is supplied through argv:

```sh
yllama-runner \
  --model /models/local/model.gguf \
  --ctx 8192 \
  --threads 4 \
  --max-tokens 128 \
  --temperature 0.8 \
  --top-p 0.95
```

The model is loaded once. The process then reads prompt frames until stdin
reaches EOF or a fatal error occurs.

## Input Frame

Each request is one prompt frame:

```text
uint32_le prompt_len
prompt bytes
```

`prompt_len` is the byte count of the prompt payload.

## Output Frames

Generated text streams as chunk frames:

```text
0x01 uint32_le chunk_len chunk_bytes
```

Successful request completion is:

```text
0x02
```

Recoverable request failure is:

```text
0x03 uint16_le message_len message_bytes
```

The message is diagnostic text for the parent process. Client-facing error
codes and structure are produced by `yllmd`.

## Cancellation

There is no in-band cancellation frame. A parent process cancels active work by
terminating the runner process and starting a fresh resident runner when needed.
