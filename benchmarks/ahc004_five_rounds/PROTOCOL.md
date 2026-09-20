# AHC004: five improvement rounds

Retrospective practice, not AHC069. Official time limit: 3 seconds.
- Baseline is the existing standalone practice solver, frozen before changes.
- Five measured development comparisons, seeds 0–9, two repeats per version.
- Confirmation uses seeds 20–39 once per version, selected before experiments.
- Freeze the selected version before confirmation; no tuning on confirmation results.
- Same C++17 -O3 -DNDEBUG flags; serial alternating version order.
- Independent cyclic-substring scorer must exactly agree with official visualizer.
- Invalid, crash, or wall time >3 seconds counts as zero, never silently rerun.
- All rounds use the same latest per-input best denominator, including every compared version.
- Raw means, iteration counts and W/T/L are secondary diagnostics, not adoption criteria.
- Archive cumulative source patches, hashes, manifests, and raw per-case scores.
- Shared local-search Runner handles schedule, acceptance and best-state retention.
- No third-party submission code is fetched. Local review summaries are structural inspiration only.
- Official editorial index has no editorial: https://atcoder.jp/contests/ahc004/editorial
