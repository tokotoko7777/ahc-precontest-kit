# Source selection

AI-training opt-outs apply to prompts and derived knowledge as well as copied code.
An independent rewrite does not make an excluded source eligible.

- Read `SOURCE_POLICY.md` and run `python3 tools/check_source_policy.py` before
  consulting practice code or old source reviews. Do not open files recorded as
  removed in `config/source_policy_exclusions.json`, including their Git history.
- Do not recover removed knowledge from mirrors, old summaries, the same author's
  alternative submissions, or lower-ranked substitutes. Keep missing cases explicit.
- For the companion `ahc-winner-techniques` archive, first run its
  `.venv/bin/python scripts/build_codex_knowledge.py`. Use its filtered corpus index
  and manifest; do not read raw private sources or legacy reviews.
- A new third-party source needs submission-level provenance and a current policy
  receipt matching its SHA-256. Page opt-outs use only the fixed Japanese/English
  notice in `div.alert.alert-warning`; explicit refusals in source code are checked
  separately. An ambiguous contest-level reference is insufficient.
- Absence of the notice is not a training or redistribution license. Keep the
  source corpus private. Do not print credentials; HTTP requests are serial and
  rate-limited.
- Keep the exclusion guard and its tests in `make verify`. Never weaken exclusions
  merely to make a build pass. The guard detects known identifiers and exact copies;
  it does not replace provenance review of newly written derivatives.
