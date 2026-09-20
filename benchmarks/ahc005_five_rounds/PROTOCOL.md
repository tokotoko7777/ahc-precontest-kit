# AHC005: five improvement rounds

Offline retrospective practice, not AHC069. Skip interactive problems in the subsequent sequence.
- Official budget 3 seconds. Existing fast deterministic practice solver is the baseline.
- Development seeds 1000–1009, two repeats per version, exactly five comparisons.
- Freeze final configuration before confirmation seeds 1020–1039, one repeat.
- These ranges avoid the already benchmarked official seeds 0–99.
- Serial alternating runs, identical C++17 -O3 -DNDEBUG flags, no concurrent heavy builds or solvers.
- Run official visualizer and an independent full route/visibility/cost checker.
- Invalid, crash, timeout, or >3 seconds counts as zero.
- Rank by a common latest per-input best vector including all versions/repeats, not raw sum alone.
- Preserve all five candidates as cumulative baseline patches and per-run hashes/raw scores.
- Use the shared local-search Runner, with Japanese TODOs for the problem-dependent interface.
- Source code is original; no third-party submission code is fetched.
- Baseline uses far less than 3 seconds; improvement is full-solver quality, not a claim of isolated Runner speedup.
