# AHC002: five measured improvement rounds

Retrospective practice/library benchmarking, not AHC069.
- Baseline: maintained TilePathProblem + TimeBasedAnnealingRunner, merged format structure.
- Development: official generator seeds 0–9. Historical scores are included by input SHA.
- Confirmation: official generator seeds 30–49, chosen before running any candidates; no tuning on them.
- Exactly five development comparisons; one incumbent versus one candidate each time.
- Alternating serial execution order, one repetition. No concurrent build or other solver during measurements.
- C++17, O3, NDEBUG; solver budget 1870 ms including initialization; official limit 2 seconds.
- Full legal-output check and exact agreement with the official visualizer for every output.
- Invalid, timed-out or over-limit runs count as zero. Diagnostics never enter best denominators.
- Use a shared historical per-input best vector including every compared variant; save raw scores and source hashes.
- Final settings frozen before the 20-case confirmation against the original baseline and legacy practice.
- No submission source collection; use the local structural review corpus as inspiration, not implementation.
- First experiment: store only a replacement segment in Move, update accepted state/cache incrementally.
