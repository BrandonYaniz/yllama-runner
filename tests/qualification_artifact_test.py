#!/usr/bin/env python3
import importlib.util
import pathlib
import tempfile
import sys

root = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("qualify", root / "qualification" / "qualify.py")
qualify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualify)

with tempfile.TemporaryDirectory() as directory:
    directory = pathlib.Path(directory)
    artifact = directory / "expected.gguf"
    artifact.write_bytes(b"known bytes")
    model = {"expected_filename": "expected.gguf",
             "expected_size_bytes": len(b"known bytes"),
             "expected_sha256": qualify.sha256(artifact)}
    assert qualify.verify_artifact(model, artifact) == model["expected_sha256"]

    wrong_name = directory / "wrong.gguf"
    wrong_name.write_bytes(b"known bytes")
    try:
        qualify.verify_artifact(model, wrong_name)
        raise AssertionError("unexpected basename accepted")
    except RuntimeError as error:
        assert "basename" in str(error)

    bad_size = dict(model, expected_size_bytes=1)
    try:
        qualify.verify_artifact(bad_size, artifact)
        raise AssertionError("unexpected size accepted")
    except RuntimeError as error:
        assert "size mismatch" in str(error)

    bad_hash = dict(model, expected_sha256="0" * 64)
    try:
        qualify.verify_artifact(bad_hash, artifact)
        raise AssertionError("unexpected hash accepted")
    except RuntimeError as error:
        assert "SHA-256 mismatch" in str(error)

    planned = {"expected_filename": None, "expected_size_bytes": None,
               "expected_sha256": None}
    try:
        qualify.verify_artifact(planned, artifact)
        raise AssertionError("unqualified planned artifact accepted")
    except RuntimeError as error:
        assert "no qualified artifact" in str(error)
