# Runner Protocol

`yllama-runner` speaks JSON Lines over stdio.

- stdin receives commands.
- stdout emits protocol events.
- stderr is reserved for logs.

Each message is one UTF-8 JSON object followed by a newline.

## Startup

On startup, the runner sends:

```json
{"type":"hello","protocol_version":1,"runner":"yllama-runner","capabilities":["generate","stream","cancel"]}
```

The supervisor sends configuration:

```json
{"type":"configure","id":"cfg-001","model_path":"/models/local/model.gguf","context_tokens":8192,"threads":4}
```

The runner loads the model and replies:

```json
{"type":"ready","id":"cfg-001","model_path":"/models/local/model.gguf","context_tokens":8192}
```

If loading fails:

```json
{"type":"error","id":"cfg-001","code":"model_load_failed","message":"Unable to load model."}
```

## Generate

```json
{"type":"generate","id":"req-001","input":{"kind":"messages","messages":[{"role":"system","content":"Answer clearly."},{"role":"user","content":"Explain this error."}]},"settings":{"temperature":0.2,"max_tokens":512}}
```

## Stream events

```json
{"type":"started","id":"req-001"}
{"type":"delta","id":"req-001","text":"The error indicates"}
{"type":"completed","id":"req-001","finish_reason":"stop","usage":{"input_tokens":42,"output_tokens":91}}
```

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

## Errors

```json
{"type":"error","id":"req-001","code":"generation_failed","message":"Generation failed."}
```

Error messages should be useful but should not include large prompt excerpts.
