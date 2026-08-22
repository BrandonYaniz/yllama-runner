#!/usr/bin/env python3
import os
import selectors
import struct
import subprocess
import sys
import time

fixture = sys.argv[1]

def envelope(kind, payload=b""):
    return bytes([kind]) + struct.pack("<I", len(payload)) + payload

def generate(prompt, mode=0):
    encoded = prompt.encode()
    payload = struct.pack("<BBI", mode, 0, 64)
    payload += struct.pack("<ddidddQ", 0, 1, 0, 0, 0, 1, 1)
    payload += encoded
    return envelope(1, payload)

def read_exact(stream, length, timeout=5):
    selector = selectors.DefaultSelector()
    selector.register(stream, selectors.EVENT_READ)
    output = bytearray()
    deadline = time.monotonic() + timeout
    while len(output) < length:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not selector.select(remaining):
            raise AssertionError("subprocess protocol timeout")
        block = os.read(stream.fileno(), length - len(output))
        if not block:
            raise EOFError
        output.extend(block)
    return bytes(output)

def read_frame(proc, timeout=5):
    header = read_exact(proc.stdout, 5, timeout)
    kind, length = struct.unpack("<BI", header)
    return kind, read_exact(proc.stdout, length, timeout)

def start():
    proc = subprocess.Popen([fixture], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            bufsize=0)
    kind, _ = read_frame(proc)
    assert kind == 0x10
    return proc

def reap(proc, expect_nonzero=False):
    try:
        code = proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2)
        raise AssertionError("runner did not exit within five seconds")
    if expect_nonzero:
        assert code != 0
    else:
        assert code == 0
    remaining = b"" if proc.stdout.closed else proc.stdout.read()
    if proc.stdin and not proc.stdin.closed:
        try: proc.stdin.close()
        except BrokenPipeError: pass
    if proc.stdout: proc.stdout.close()
    if proc.stderr: proc.stderr.close()
    return remaining

# Fatal oversized framing cannot resynchronize arbitrary trailing bytes.
proc = start()
proc.stdin.write(b"\x01" + struct.pack("<I", (32 << 20) + 1) + generate("after"))
proc.stdin.flush()
kind, _ = read_frame(proc)
assert kind == 0x03
assert reap(proc, expect_nonzero=True) == b""

# Fatal framing interrupts an already active generation.
proc = start()
proc.stdin.write(generate("slow")); proc.stdin.flush()
assert read_frame(proc)[0] == 0x01
proc.stdin.write(b"\x01" + struct.pack("<I", (32 << 20) + 1) + b"junk")
proc.stdin.flush()
while True:
    kind, _ = read_frame(proc)
    if kind == 0x03:
        break
    assert kind == 0x01
assert reap(proc, expect_nonzero=True) == b""

# Fatal backend error exits although the parent keeps stdin open.
proc = start()
proc.stdin.write(generate("fatal")); proc.stdin.flush()
kind, _ = read_frame(proc)
assert kind == 0x03
assert reap(proc, expect_nonzero=True) == b""

# Unknown messages and invalid requests are recoverable.
proc = start()
proc.stdin.write(envelope(0x7f, b"unknown") + generate("bad", mode=2) + generate("ok"))
proc.stdin.flush()
assert read_frame(proc)[0] == 0x03
assert read_frame(proc)[0] == 0x03
while True:
    kind, _ = read_frame(proc)
    if kind == 0x04: break
    assert kind == 0x01
proc.stdin.close()
reap(proc)

# Active cancellation completes once and the same process handles a successor.
proc = start(); pid = proc.pid
proc.stdin.write(generate("slow")); proc.stdin.flush()
assert read_frame(proc)[0] == 0x01
proc.stdin.write(envelope(2)); proc.stdin.flush()
while True:
    kind, payload = read_frame(proc)
    if kind == 0x04:
        assert payload[0] == 3
        break
    assert kind == 0x01
assert proc.poll() is None and proc.pid == pid
proc.stdin.write(generate("after")); proc.stdin.flush()
while read_frame(proc)[0] != 0x04: pass
proc.stdin.close()
reap(proc)

# Idle Cancel is a harmless no-op and cannot pollute the next response.
proc = start()
proc.stdin.write(envelope(2) + generate("after")); proc.stdin.flush()
while read_frame(proc)[0] != 0x04: pass
proc.stdin.close()
reap(proc)

# Closing stdout cannot leave the runner waiting on still-open stdin.
proc = start()
proc.stdout.close()
proc.stdin.write(generate("ok")); proc.stdin.flush()
reap(proc, expect_nonzero=True)
