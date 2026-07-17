#!/usr/bin/env python3
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / "qualification" / "models.json").read_text(encoding="utf-8"))
assert manifest["synchronized_yllmd_commit"] == "c9fbab3118e6a2e917fd79971ea69b2f593bbb72"
assert manifest["synchronized_catalog_revision"] == "2026.07.7-draft"
expected = {
    "phi4-mini-instruct": "<|system|>Be concise.<|end|><|user|>Hello.<|end|><|assistant|>",
    "gemma3-1b-it": "<bos><start_of_turn>user\nBe concise.\n\nHello.<end_of_turn>\n<start_of_turn>model\n",
    "llama32-1b-instruct": "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\nBe concise.<|eot_id|><|start_header_id|>user<|end_header_id|>\n\nHello.<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n",
    "qwen3-1.7b": "<|im_start|>system\nBe concise.<|im_end|>\n<|im_start|>user\nHello.<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n",
    "qwen25-coder-1.5b-instruct": "<|im_start|>system\nBe concise.<|im_end|>\n<|im_start|>user\nWrite hello world.<|im_end|>\n<|im_start|>assistant\n",
    "mistral-nemo-12b-instruct": "<s>[INST]Be concise.\n\nHello.[/INST]",
    "granite3.3-2b-instruct": "<|start_of_role|>system<|end_of_role|>Be concise.<|end_of_text|>\n<|start_of_role|>user<|end_of_role|>Hello.<|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>",
}
entries = {entry["catalog_variant_id"]: entry for entry in manifest["models"]}
assert entries.keys() == expected.keys()
for variant, prompt in expected.items():
    entry = entries[variant]
    assert entry["prompt"].encode("utf-8") == prompt.encode("utf-8")
    for field in ("catalog_variant_id", "expected_filename", "expected_size_bytes",
                  "expected_sha256", "prompt_template_id"):
        assert field in entry
for entry in entries.values():
    assert entry["expected_size_bytes"] > 0
    assert len(entry["expected_sha256"]) == 64

qwen3 = entries["qwen3-1.7b"]
assert qwen3["expected_filename"] == "Qwen3-1.7B-Q8_0.gguf"
assert qwen3["quantization"] == "Q8_0"
assert qwen3["prompt_template_id"] == "qwen3-nonthinking-chatml"
