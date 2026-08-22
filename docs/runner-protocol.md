# Runner protocol

The runner is a child process with one model. It reads messages from stdin,
writes events to stdout, and reserves stderr for diagnostics. It never opens a
socket. Closing stdin is the only shutdown command.

## Framing

Every message in either direction has the same header:

```text
uint8  type
uint32 payload_length_le
bytes  payload
```

The maximum payload is 32 MiB. All integers are little-endian, floating-point
values are IEEE-754 `float64`, and all text is UTF-8.

## Startup

Start the process with:

```text
yllama-runner --model PATH --ctx TOKENS --threads N [--gpu-layers N]
```

After loading the model, the runner writes Ready (`0x10`) with an empty payload.
A load failure writes Error and exits nonzero. The parent should impose a model
load timeout.

## Input

Generate is type `0x01`:

```text
uint8  tokenization_mode       # 0 raw, 1 preformatted
uint8  stop_count
uint32 max_tokens
float64 temperature
float64 top_p
int32  top_k
float64 min_p
float64 presence_penalty
float64 repeat_penalty
uint64 seed                    # UINT64_MAX selects runner randomness
repeat stop_count times:
  uint32 stop_length
  bytes  stop_utf8
bytes prompt_utf8              # all remaining payload bytes
```

Raw tokenization adds model-required special tokens but does not interpret
special-token-looking prompt text. Preformatted tokenization recognizes special
tokens and does not add another beginning token.

Cancel is type `0x02` with an empty payload. During generation it produces a
Completed event with finish reason `cancelled`; while idle it is a no-op. The
same process can accept another Generate afterward. Requests are sequential.

## Output

Chunk (`0x01`) contains only the next UTF-8 text bytes.

Error (`0x03`) contains:

```text
uint16 code_length
bytes  code
bytes  message                 # all remaining payload bytes
```

Completed (`0x04`) contains exactly nine bytes:

```text
uint8  finish_reason           # 0 eos, 1 length, 2 stop, 3 cancelled
uint32 input_tokens
uint32 output_tokens
```

Each Generate ends with exactly one Completed or Error. Fatal backend or framing
errors are followed by process exit; request-specific errors leave the model
resident.

## Limits

- Prompt: 16 MiB.
- Stop strings: at most 64 and 64 KiB total.
- Error code: 128 bytes.
- Error message: 16 KiB.

Declared lengths are checked before allocation. A truncated or oversized frame
is fatal because the stream can no longer be synchronized.
