# Runner Protocol

`yllama-runner` accepts JSON Lines commands over stdio.

- stdin receives commands.
- stdout emits protocol events and, when requested, raw generated text.
- stderr is reserved for logs.

Each stdin message is one UTF-8 JSON object followed by a newline.

The runner handles one configured model and one active generation request at a time. A supervisor may send `cancel` while generation is active; other commands sent during active generation are rejected with `request_active`.

## Command ids

Every command has an `id` string. The runner copies that id into response events so a supervisor can match events to commands.

The `cancel` command is the exception to normal command-response pairing: its `id` must be the id of the active `generate` request, not a new cancellation id.

## Startup

On startup, the runner sends:

```json
{"type":"hello","protocol_version":1,"runner":"yllama-runner","capabilities":["generate","stream","cancel","output_modes"]}
```

Fields:

- `protocol_version`: currently `1`.
- `runner`: currently `yllama-runner`.
- `capabilities`: command/event features supported by this runner.

## Configure

The supervisor sends configuration:

```json
{"type":"configure","id":"cfg-001","model_path":"/models/local/model.gguf","context_tokens":8192,"threads":4}
```

Fields:

- `model_path`: local filesystem path to a GGUF model.
- `context_tokens`: requested llama context size. Must be greater than zero.
- `threads`: requested llama worker thread count. Must be greater than zero.

The runner loads the model and replies:

```json
{"type":"ready","id":"cfg-001","model_path":"/models/local/model.gguf","context_tokens":8192}
```

The returned `context_tokens` is the actual context size reported by llama.cpp after context initialization.

If loading fails:

```json
{"type":"error","id":"cfg-001","code":"model_load_failed","message":"Unable to load model."}
```

## Generate

```json
{"type":"generate","id":"req-001","input":{"kind":"messages","messages":[{"role":"system","content":"Answer clearly."},{"role":"user","content":"Explain this error."}]},"settings":{"temperature":0.2,"max_tokens":512}}
```

Input forms:

- `{"kind":"prompt","prompt":"..."}` sends raw prompt text.
- `{"kind":"messages","messages":[...]}` sends role/content messages. The current renderer formats each message as `role: content` and appends `assistant:`.

Settings:

- `temperature`: optional number. Defaults to `0.8`. Use `0` for greedy decoding.
- `top_p`: optional number in `(0, 1]`. Defaults to `0.95` when sampling is enabled.
- `max_tokens`: optional integer greater than zero. Defaults to `128`.
- `output`: optional object that selects generation output shape. Defaults to `{"format":"json","delivery":"stream"}`.
- `stream`: optional legacy boolean. Defaults to `true`. Set to `false` to receive JSON completed output. Ignored when `output` is present.
- `stop`: optional array of strings. Matching stop text is not emitted in `delta` events.

Output settings:

- `output.format`: `json` or `text`.
- `output.delivery`: `stream` or `complete`.

The four supported combinations are:

- `{"format":"json","delivery":"stream"}`: emits JSON `started`, JSON `delta` events, then JSON `completed`.
- `{"format":"json","delivery":"complete"}`: emits JSON `started`, then one JSON `completed` event with `text`.
- `{"format":"text","delivery":"stream"}`: writes raw generated text chunks as they arrive.
- `{"format":"text","delivery":"complete"}`: writes raw generated text after generation finishes.

Protocol failures and cancellation are still reported as JSON events.

## JSON Generate Events

```json
{"type":"started","id":"req-001"}
{"type":"delta","id":"req-001","text":"The error indicates"}
{"type":"completed","id":"req-001","finish_reason":"stop","usage":{"input_tokens":42,"output_tokens":91}}
```

For compact output, send `"stream":false` in `settings`:

```json
{"type":"generate","id":"req-001","input":{"kind":"prompt","prompt":"Write one sentence."},"settings":{"stream":false}}
```

The generated text is returned in one terminal event:

```json
{"type":"completed","id":"req-001","finish_reason":"stop","usage":{"input_tokens":42,"output_tokens":91},"text":"One generated sentence."}
```

The equivalent explicit output setting is:

```json
{"type":"generate","id":"req-001","input":{"kind":"prompt","prompt":"Write one sentence."},"settings":{"output":{"format":"json","delivery":"complete"}}}
```

JSON event order:

- `started` is emitted once generation begins.
- zero or more `delta` events stream generated text when JSON stream output is selected.
- exactly one terminal event follows: `completed`, `cancelled`, or `error`.

## Text Output

Text output writes only generated text for successful generation. It does not emit `started`, `delta`, or `completed` events for that request.

```json
{"type":"generate","id":"req-001","input":{"kind":"prompt","prompt":"Write one sentence."},"settings":{"output":{"format":"text","delivery":"stream"}}}
```

With `delivery:"stream"`, stdout receives generated text chunks immediately. With `delivery:"complete"`, stdout receives the same raw text after generation finishes.

`completed.finish_reason` values:

- `stop`: generation hit an end-of-generation token or configured stop string.
- `length`: generation reached `max_tokens` or the configured context limit.

`usage.input_tokens` and `usage.output_tokens` are reported from llama tokenization/generation counts.

## Cancel

```json
{"type":"cancel","id":"req-001"}
```

The `id` must match the active generation request. If cancellation succeeds, the runner emits `cancelled` after the active backend observes the cancellation request:

```json
{"type":"cancelled","id":"req-001"}
```

If no active request matches the id:

```json
{"type":"error","id":"req-001","code":"request_not_active","message":"No active request matched the cancel command."}
```

## Shutdown

```json
{"type":"shutdown","id":"shutdown-001"}
```

The runner should finish any cleanup and exit with status code 0.

If a generation request is active, shutdown waits for it to finish. Send `cancel` first when prompt shutdown is required.

## Errors

```json
{"type":"error","id":"req-001","code":"generation_failed","message":"Generation failed."}
```

Error messages should be useful but should not include large prompt excerpts.

Common error codes:

- `invalid_json`: input line was not valid JSON.
- `invalid_command`: command shape or field type was invalid.
- `unknown_command`: command `type` was not recognized.
- `not_configured`: generation was requested before a successful `configure`.
- `request_active`: a second configure or generate command arrived while generation was active.
- `request_not_active`: a cancel command did not match the active request.
- `invalid_config`: configuration values were not acceptable.
- `model_load_failed`: llama.cpp could not load the requested model.
- `context_init_failed`: llama.cpp could not initialize a context.
- `invalid_settings`: generation settings were outside accepted ranges.
- `tokenize_failed`: prompt tokenization failed.
- `context_exceeded`: prompt did not fit in the configured context.
- `sampler_init_failed`: sampler setup failed.
- `decode_failed`: llama.cpp failed while decoding.
- `detokenize_failed`: token text conversion failed.
