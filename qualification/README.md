# Curated model qualification

The fixtures are synchronized to yllmd commit
`c9fbab3118e6a2e917fd79971ea69b2f593bbb72`, embedded catalog revision
`2026.07.7-draft`. The manifest strings are byte fixtures and the contract test
compares every UTF-8 byte.

Set each environment variable named in `models.json` to the exact local GGUF,
then run `python3 qualification/qualify.py --runner build/yllama-runner
--results qualification-results.json`. This opt-in release gate hashes every
artifact, uses the recorded yllmd preformatted syntax, performs two deterministic
requests in one process, validates UTF-8/terminal frames/usage, and records the
platform, runner, llama.cpp revision, and failure details. The first configured
model waits for streamed output, performs in-band cancellation, verifies reason
`cancelled`, then successfully generates again in the same PID.
Ordinary tests never download these models.
