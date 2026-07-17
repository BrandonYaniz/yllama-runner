# Curated model qualification

The fixtures are synchronized to yllmd commit
`3a9b417a70eeaa103e49de9cc53eda0b957313b0`, embedded catalog revision
`2026.07.6-draft`. The manifest strings are byte fixtures and the contract test
compares every UTF-8 byte. Qwen3 1.7B is still planned in that catalog, so its
missing artifact identity is explicit and qualification fails rather than
accepting an unverified file.

Set each environment variable named in `models.json` to the exact local GGUF,
then run `python3 qualification/qualify.py --runner build/yllama-runner
--results qualification-results.json`. This opt-in release gate hashes every
artifact, uses the recorded yllmd preformatted syntax, performs two deterministic
requests in one process, validates UTF-8/terminal frames/usage, and records the
platform, runner, llama.cpp revision, and failure details. The first configured
model waits for streamed output, performs in-band cancellation, verifies reason
`cancelled`, then successfully generates again in the same PID.
Ordinary tests never download these models.
