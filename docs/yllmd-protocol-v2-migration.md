# yllmd migration to runner protocol 2

This document is the client contract for yllama-runner `26.07.16.01-Release`.
Runner release versions and wire versions are independent. Release versions use
`YY.MM.DD.NN` with optional `-Release`; the catalog must compare the complete
string. Protocol 1 remains the default during migration. yllmd must pass
`--protocol 2`; protocol 1 will be removed no earlier than two catalog releases
after every supported yllmd channel has moved to protocol 2.

## Process startup and readiness

Start the child with:

```text
yllama-runner --protocol 2 --model PATH --ctx TOKENS --threads N --gpu-layers N
```

`--gpu-layers` defaults to 0 (CPU-only); `-1` requests maximum/automatic
llama.cpp offload, and positive values request that many layers. Values below
`-1` are invalid. Model path, context, threads, GPU layers,
and protocol are process-lifetime settings. All generation settings are in each
Generate. Diagnostics are UTF-8 on stderr and must never be parsed as protocol.

The first stdout frame is either Ready or Error. Ready is written only after the
model and context load. A startup Error is followed by nonzero exit. Client
detection rules:

- Ready that parses, selects version 2, and contains required capabilities is a
  clean startup.
- Error before Ready is a load failure; surface its code and message and await
  nonzero exit.
- A truncated frame, invalid length, wrong first type, mismatched version, or
  invalid UTF-8 version is malformed Ready; terminate the child.
- Exit status 2 plus stderr containing `unsupported protocol` means the runner
  rejected the selection before model loading (there is no stdout protocol yet).
- EOF or child exit at any other point is an unexpected runner exit.
- yllmd should impose a configurable startup timeout (recommended 120 seconds),
  kill the child on expiry, and report `runner_startup_timeout`.

`--version` prints the release version. `--build-info` prints release version,
supported protocols, pinned llama.cpp revision, compiled compute backends, and
target OS/architecture without loading a model.

## Common framing and numeric encoding

Both directions use:

```text
uint8  message_type
uint32 payload_length_le
bytes  payload[payload_length]
```

Integers are unsigned unless named `int32`; all are little-endian. `float64` is
IEEE-754 binary64 encoded little-endian. Text is UTF-8; error codes are stable
lowercase ASCII with underscores. No padding or alignment bytes occur.

Limits are constants in `src/frame.hpp`: outer payload 32 MiB, prompt 16 MiB,
64 stop strings, 64 KiB total stop bytes (and per stop), error code 128 bytes,
and error message 16 KiB. Lengths are validated before allocation. Empty stop
strings are invalid. Unknown message types are recoverable when their declared
payload was completely read; malformed/truncated framing is fatal from the
client's perspective even if an Error was received. The runner emits at most
one structured Error for a truncated envelope/payload, oversized declared
frame, or other loss of synchronization, flushes, and exits nonzero without
attempting another parse.

## Input messages

Types are Generate `0x01`, Cancel `0x02`, and Shutdown `0x03`. Cancel and
Shutdown payloads are empty.

Generate payload:

```text
uint8  tokenization_mode       # 0 raw, 1 preformatted
uint8  flags                   # must be zero
uint16 stop_count_le
uint32 max_tokens_le
float64 temperature_le
float64 top_p_le
int32  top_k_le
float64 min_p_le
float64 presence_penalty_le
float64 repeat_penalty_le
uint64 seed_le                 # UINT64_MAX = nondeterministic
repeat stop_count times:
  uint32 stop_length_le
  bytes  stop_utf8
uint32 prompt_length_le
bytes  prompt_utf8
```

Validation: `max_tokens` 1..1,000,000; temperature finite 0..100; top-p finite
and `(0,1]`; top-k 0..1,000,000 (0 disables); min-p finite `[0,1]`; presence
penalty finite `[-2,2]`; repeat penalty finite `(0,100]`. All other NaN and
infinite values are rejected. llama.cpp consumes the low 32 bits of an explicit
64-bit seed; yllmd should use values at most `UINT32_MAX` for portability.

Raw mode invokes the pinned `llama_tokenize` with `add_special=true` and
`parse_special=false`: model-required initial tokens are added and arbitrary
control-token-looking user text remains text. Preformatted mode uses
`add_special=false`, `parse_special=true`: recognized vocabulary control tokens
are honored and no BOS is injected. Input usage is the exact token vector sent
to llama.cpp, including any special tokens produced by those rules.

## Output messages

Types are Chunk `0x01`, Error `0x03`, Completed `0x04`, Ready `0x10`.

Chunk payload is `uint32 byte_length_le` followed by exactly that many UTF-8
bytes. The inner length must equal outer payload length minus four. Every Chunk
is independently valid UTF-8. The runner buffers incomplete code points and
never substitutes U+FFFD. Stop bytes are retained until they cannot begin a
match, so a matched stop is never emitted.

Error payload is `uint16 code_length_le`, code bytes, `uint16
message_length_le`, message bytes. Stable request codes include
`malformed_frame`, `frame_too_large`, `malformed_message`,
`unknown_message_type`, `malformed_generate`, `invalid_reserved`,
`invalid_tokenization_mode`, `too_many_stops`, `invalid_stop_sequence`,
`invalid_prompt_utf8`,
`prompt_too_large`, `invalid_max_tokens`, `invalid_temperature`,
`invalid_top_p`, `invalid_top_k`, `invalid_min_p`,
`invalid_presence_penalty`, `invalid_repeat_penalty`, `no_active_request`,
`tokenize_failed`, `context_exceeded`, `sampler_init_failed`, and
`invalid_backend_utf8`. Startup/backend codes include `model_load_failed`,
`context_init_failed`, `decode_failed`, and `detokenize_failed`. Treat unknown
codes as opaque future codes.

Completed payload (exactly 25 bytes):

```text
uint8  finish_reason           # 0 eos, 1 length, 2 stop, 3 cancelled
uint32 input_tokens_le
uint32 output_tokens_le
uint64 prompt_microseconds_le
uint64 generation_microseconds_le
```

Exactly one Completed terminates every successful Generate. An Error terminates
a failed request and has no Completed. Request errors are recoverable; fatal
backend/output errors are followed by process exit. Timing uses a monotonic
clock. Output tokens count sampled non-EOS tokens, including buffered tokens
later suppressed by a stop sequence.

Backend errors carry an internal recoverable/fatal disposition. Invalid request,
tokenization, context-fit, and request-specific sampler errors are recoverable.
Decode, detokenization, invalid backend UTF-8, and backend invariant failures
are fatal and cause a flushed Error followed by nonzero exit.

Ready payload:

```text
uint16 protocol_version_le
uint16 runner_version_length_le
bytes  runner_version
uint32 actual_context_size_le
uint64 capabilities_le
```

Capabilities: bit 0 structured completion, 1 per-request sampling, 2
tokenization modes, 3 cancellation, 4 stop sequences, 5 usage counters, 6
timing metadata. Unknown bits must be ignored. yllmd must require bits 0..6.

## Sequences

Normal request: wait for Ready; send Generate; consume zero or more Chunks; then
consume exactly one Completed or one Error. Do not send concurrent Generates;
sequential Generates reuse the resident model and may use different settings.

Cancellation: while a Generate is active, send empty Cancel. Continue reading;
the terminal frame is Completed with reason 3. Then another Generate may be
sent. An idle Cancel returns `no_active_request`. There is one reader thread and
one generation thread; no thread is created per request.

Shutdown: only while idle, send empty Shutdown, close stdin, and expect clean
exit status 0. EOF while idle is also clean. If yllmd must abort a hung backend,
terminate the process; that is distinct from in-band cancellation.

## Byte fixtures

Spaces below separate bytes; fixtures are also asserted by `frame_test`.

```text
Cancel:   02 00 00 00 00
Shutdown: 03 00 00 00 00
Chunk "hi": 01 06 00 00 00 02 00 00 00 68 69
Completed(length,in=2,out=3,prompt_us=4,generation_us=5):
04 19 00 00 00 01 02 00 00 00 03 00 00 00
04 00 00 00 00 00 00 00 05 00 00 00 00 00 00 00
Ready(protocol=2,version="x",ctx=4096,caps=0x7f):
10 11 00 00 00 02 00 01 00 78 00 10 00 00
7f 00 00 00 00 00 00 00
```

## Go-oriented pseudocode

```go
func writeFrame(w io.Writer, typ byte, payload []byte) error {
    if len(payload) > 32<<20 { return errTooLarge }
    var h [5]byte; h[0] = typ
    binary.LittleEndian.PutUint32(h[1:], uint32(len(payload)))
    if _, err := w.Write(h[:]); err != nil { return err }
    _, err := w.Write(payload); return err
}

func readFrame(r io.Reader) (byte, []byte, error) {
    var h [5]byte
    if _, err := io.ReadFull(r, h[:]); err != nil { return 0, nil, err }
    n := binary.LittleEndian.Uint32(h[1:])
    if n > 32<<20 { return 0, nil, errTooLarge }
    p := make([]byte, n)
    _, err := io.ReadFull(r, p)
    return h[0], p, err
}
// Encode float64 with math.Float64bits and PutUint64. Validate with
// math.IsNaN/math.IsInf before sending. Use io.ReadFull and a write-all loop.
```

## yllmd checklist

- Select a catalog runner release independently from protocol version.
- Pass `--protocol 2` and process-lifetime flags only.
- Enforce the startup timeout and validate Ready/capabilities/context.
- Implement bounded, exact reads/writes and every limit above.
- Move sampling, seed, stops, and tokenization mode into Generate.
- Send yllmd-rendered chat templates as preformatted mode.
- Accept only valid Chunk UTF-8 and one terminal frame per request.
- Map finish reasons, usage, and timings into yllmd responses.
- Use Cancel without killing the resident runner; test a subsequent request.
- Send Shutdown during graceful yllmd teardown.
- Log stderr separately and include build info in support diagnostics.
- Keep a protocol-1 fallback only during the catalog migration window.
