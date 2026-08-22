#!/usr/bin/env python3
"""Process-safe, catalog-exact runner release qualification."""
import argparse
import hashlib
import json
import os
import pathlib
import platform
import selectors
import struct
import subprocess
import threading
import time

FRAME_LIMIT = 32 << 20
STARTUP_TIMEOUT = 120.0
REQUEST_TIMEOUT = 60.0
CANCEL_TIMEOUT = 20.0
SHUTDOWN_TIMEOUT = 10.0
STDERR_LIMIT = 64 << 10

def frame(kind, payload=b""):
    return bytes([kind]) + struct.pack("<I", len(payload)) + payload

def generate(prompt, seed=1, max_tokens=32):
    payload = struct.pack("<BBI", 1, 0, max_tokens)
    payload += struct.pack("<ddidddQ", 0.0, 1.0, 0, 0.0, 0.0, 1.0, seed)
    encoded = prompt.encode("utf-8")
    return frame(1, payload + encoded)

class FrameReader:
    def __init__(self, stream):
        self.fd = stream.fileno()
        self.buffer = bytearray()
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.fd, selectors.EVENT_READ)

    def exact(self, length, deadline):
        while len(self.buffer) < length:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise TimeoutError("protocol read timeout")
            block = os.read(self.fd, min(65536, length - len(self.buffer)))
            if not block:
                raise RuntimeError("unexpected runner EOF")
            self.buffer.extend(block)
        result = bytes(self.buffer[:length])
        del self.buffer[:length]
        return result

    def frame(self, timeout):
        deadline = time.monotonic() + timeout
        kind, length = struct.unpack("<BI", self.exact(5, deadline))
        if length > FRAME_LIMIT:
            raise RuntimeError("oversized runner output")
        return kind, self.exact(length, deadline)

class StderrCollector:
    def __init__(self, stream):
        self.stream = stream
        self.data = bytearray()
        self.thread = threading.Thread(target=self._run, daemon=False)
        self.thread.start()

    def _run(self):
        for block in iter(lambda: self.stream.read(4096), b""):
            if len(self.data) < STDERR_LIMIT:
                self.data.extend(block[:STDERR_LIMIT - len(self.data)])

    def finish(self):
        self.thread.join(timeout=2)
        return self.data.decode("utf-8", errors="replace")

def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()

def send(proc, data):
    if proc.stdin is None:
        raise RuntimeError("runner stdin unavailable")
    proc.stdin.write(data)
    proc.stdin.flush()

def completed_payload(payload):
    if len(payload) != 9:
        raise RuntimeError("invalid Completed length")
    return struct.unpack("<BII", payload)

def request(proc, reader, prompt, timeout=REQUEST_TIMEOUT):
    send(proc, generate(prompt))
    chunks, terminals = [], 0
    deadline = time.monotonic() + timeout
    while True:
        kind, payload = reader.frame(max(0.001, deadline - time.monotonic()))
        if kind == 1:
            payload.decode("utf-8")
            chunks.append(payload)
        elif kind == 4:
            terminals += 1
            reason, input_tokens, output_tokens = completed_payload(payload)
            if terminals != 1 or reason not in (0, 1, 2):
                raise RuntimeError("invalid terminal completion")
            if input_tokens <= 0 or output_tokens <= 0 or not b"".join(chunks):
                raise RuntimeError("invalid output or usage")
            return
        elif kind == 3:
            raise RuntimeError("runner Error: " + repr(payload))
        else:
            raise RuntimeError("unexpected output type " + str(kind))

def cancel_after_chunk(proc, reader, prompt):
    send(proc, generate(prompt, seed=11, max_tokens=100000))
    sent_cancel = False
    terminals = 0
    deadline = time.monotonic() + CANCEL_TIMEOUT
    while True:
        kind, payload = reader.frame(max(0.001, deadline - time.monotonic()))
        if kind == 1:
            payload.decode("utf-8")
            if not sent_cancel:
                send(proc, frame(2))
                sent_cancel = True
        elif kind == 4:
            terminals += 1
            reason, _, _ = completed_payload(payload)
            if not sent_cancel:
                raise RuntimeError("generation completed before cancellation")
            if terminals != 1 or reason != 3:
                raise RuntimeError("Cancel did not produce exactly one cancelled completion")
            return
        elif kind == 3:
            raise RuntimeError("Cancel produced Error: " + repr(payload))

def stop_process(proc, graceful=False):
    if proc is None:
        return
    if proc.poll() is None and graceful:
        try:
            proc.stdin.close()
            proc.wait(timeout=SHUTDOWN_TIMEOUT)
        except Exception:
            pass
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=3)
    if proc.stdin is not None and not proc.stdin.closed:
        proc.stdin.close()
    if proc.stdout is not None:
        proc.stdout.close()

def verify_artifact(model, path):
    required = (model.get("expected_filename"), model.get("expected_size_bytes"),
                model.get("expected_sha256"))
    if any(value is None for value in required):
        raise RuntimeError("yllmd catalog has no qualified artifact for this planned variant")
    if pathlib.Path(path).name != model["expected_filename"]:
        raise RuntimeError("unexpected basename: " + pathlib.Path(path).name)
    actual_size = os.path.getsize(path)
    if actual_size != model["expected_size_bytes"]:
        raise RuntimeError(f"size mismatch: {actual_size}")
    actual_hash = sha256(path)
    if actual_hash != model["expected_sha256"]:
        raise RuntimeError("SHA-256 mismatch: " + actual_hash)
    return actual_hash

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True)
    parser.add_argument("--manifest", default=os.path.join(os.path.dirname(__file__), "models.json"))
    parser.add_argument("--results", required=True)
    args = parser.parse_args()
    manifest = json.load(open(args.manifest, encoding="utf-8"))
    build_info = subprocess.check_output([args.runner, "--build-info"], text=True, timeout=10)
    revision = next(line.split(": ", 1)[1] for line in build_info.splitlines() if line.startswith("llama.cpp-revision:"))
    version = next(line.split(": ", 1)[1] for line in build_info.splitlines() if line.startswith("runner-version:"))
    results = []
    for index, model in enumerate(manifest["models"]):
        path = os.environ.get(model["env"])
        command = [args.runner, "--model", path or "<unset>", "--ctx", "2048", "--threads", str(os.cpu_count() or 1)]
        record = {
            "model_family": model["family"], "catalog_variant_id": model["catalog_variant_id"],
            "expected_filename": model["expected_filename"], "expected_size_bytes": model["expected_size_bytes"],
            "expected_sha256": model["expected_sha256"], "prompt_template_id": model["prompt_template_id"],
            "quantization": model["quantization"], "llama_cpp_revision": revision,
            "runner_version": version, "os": platform.system(), "architecture": platform.machine(),
            "command": command, "build_info": build_info.strip(), "passed": False, "pid": None,
        }
        proc = reader = collector = None
        graceful = False
        try:
            if not path:
                raise RuntimeError("unset " + model["env"])
            record["artifact_sha256"] = verify_artifact(model, path)
            command[2] = path
            proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
            record["pid"] = proc.pid
            reader = FrameReader(proc.stdout)
            collector = StderrCollector(proc.stderr)
            kind, _ = reader.frame(STARTUP_TIMEOUT)
            if kind != 0x10:
                raise RuntimeError("missing Ready")
            request(proc, reader, model["prompt"])
            request(proc, reader, model["prompt"])
            if index == 0:
                original_pid = proc.pid
                cancel_after_chunk(proc, reader, model["prompt"])
                if proc.poll() is not None or proc.pid != original_pid:
                    raise RuntimeError("runner did not remain resident after cancellation")
                request(proc, reader, model["prompt"])
            graceful = True
            stop_process(proc, graceful=True)
            graceful = False
            if proc.returncode != 0:
                raise RuntimeError("unclean shutdown: " + str(proc.returncode))
            record["passed"] = True
        except Exception as exc:
            record["failure_details"] = f"{type(exc).__name__}: {exc}; command={command!r}; build_info={build_info.strip()!r}"
        finally:
            stop_process(proc, graceful=graceful)
            if collector is not None:
                record["stderr"] = collector.finish()
            results.append(record)
    with open(args.results, "w", encoding="utf-8") as stream:
        json.dump({"generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                   "synchronized_yllmd_commit": manifest["synchronized_yllmd_commit"],
                   "synchronized_catalog_revision": manifest["synchronized_catalog_revision"],
                   "results": results}, stream, indent=2)
    raise SystemExit(0 if all(result["passed"] for result in results) else 1)

if __name__ == "__main__":
    main()
