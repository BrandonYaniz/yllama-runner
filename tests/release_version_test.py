#!/usr/bin/env python3
import pathlib
import re
import subprocess
import sys

runner = pathlib.Path(sys.argv[1])
root = pathlib.Path(sys.argv[2])
source = (root / "VERSION").read_text(encoding="utf-8").strip()
version_output = subprocess.check_output([runner, "--version"], text=True).strip()
build_info = subprocess.check_output([runner, "--build-info"], text=True)
documentation = (root / "docs" / "versioning.md").read_text(encoding="utf-8")
workflow = (root / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
match = re.search(r"YLLAMA_EXPECTED_RELEASE_VERSION:\s*([^\s]+)", workflow)
if match is None:
    raise SystemExit("CI expected release version is missing")
ci_version = match.group(1).strip('"\'')
checks = {
    "--version": version_output == f"yllama-runner {source}",
    "--build-info": f"runner-version: {source}\n" in build_info,
    "version documentation": source in documentation,
    "CI expected version": ci_version == source,
}
failures = [name for name, passed in checks.items() if not passed]
if failures:
    raise SystemExit("release version mismatch: " + ", ".join(failures))

base = [runner, "--gpu-layers"]
for value in ("-1", "0", "3"):
    result = subprocess.run(base + [value], capture_output=True, text=True,
                            timeout=5)
    if (result.returncode != 2 or "usage:" not in result.stderr or
            "--gpu-layers must" in result.stderr):
        raise SystemExit("valid --gpu-layers value rejected: " + value)
invalid = subprocess.run(base + ["-2"], capture_output=True, text=True,
                         timeout=5)
if invalid.returncode != 2 or "--gpu-layers must" not in invalid.stderr:
    raise SystemExit("invalid negative --gpu-layers value was accepted")
