# AHC001: five improvement rounds

User requested five measured improvement loops per problem, beginning with AHC001.
This is retrospective library/model benchmarking, not the current AHC069 contest.

- Baseline: the existing RegionProblem with TimeBasedAnnealingRunner from the search-family PR.
- Official tool directory: /tmp/ahc001-official.E0EJxN/tools.
- Official system seed manifest: /tmp/ahc001-official.E0EJxN/system/seeds.txt.
- Manifest MD5: 8fc1ce3f4beabac6abc1bdb4206d7f7e.
- Development: manifest rows 100–109, ten cases. These are not seeds 100–109.
- Confirmation: manifest rows 200–219, twenty separate cases; no tuning on them.
- Historical threshold-table experiments already cover rows 100–119. Development rows 100–109 therefore have historical results, which are included in the common best vector. Confirmation starts at 200 to avoid these prior measured sets.
- Each round compares the incumbent and one candidate, with alternating execution order.
- Compiler flags: C++17, O3, NDEBUG, Wall, Wextra; solver budget 4750 ms, limit 5 seconds.
- All compilation finishes before sequential solver measurements. No parallel solvers.
- Every result is checked for legality and against both the official visualizer and independent score calculation.
- Invalid, crashed, or over-5-second runs count as zero. No speed-only adoption claims.
- Each run directory retains flattened source, input, output, stderr, manifest, and raw CSV.
- Round selection uses the common per-case best denominator; final report recomputes all measured development versions against one latest best vector, including matching historical data when available.
- The final candidate is compared against the original baseline on the confirmation set. A ten-case development win alone is not evidence of general improvement.
- Existing problem-specific experiments and the abandoned submission collector remain outside the merged format PR.

## Planned first experiment

Replace the near-zero-temperature final phase with the shared strict hill-climbing mode. The first phase and all neighborhoods remain unchanged.
Subsequent rounds will be selected after examining the preceding results.
