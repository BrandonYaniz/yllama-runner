# Curated model qualification

Set each environment variable named in `models.json` to the exact local GGUF,
then run `python3 qualification/qualify.py --runner build/yllama-runner
--results qualification-results.json`. This opt-in release gate hashes every
artifact, uses the recorded yllmd preformatted syntax, performs two deterministic
requests in one process, validates UTF-8/terminal frames/usage, and records the
platform, runner, llama.cpp revision, and failure details. The first configured
model also performs an in-band cancellation and verifies reason `cancelled`.
Ordinary tests never download these models.
