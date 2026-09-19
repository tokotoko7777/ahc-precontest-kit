# AHC003: five improvement rounds

Retrospective practice, not AHC069. Official limit 2 seconds.
- Existing practice (hierarchical normalized-gradient estimator) is the original baseline.
- Use local-search/basic's TimeBasedAnnealingRunner in hill-climbing mode to fit a regularized model of observed costs; Dijkstra chooses each route.
- Objective during fitting is negative weighted squared error, not the official score. Adoption uses the official interactive score only.
- Development seeds 0–9; fixed confirmation seeds 20–39; chosen before experiments.
- Exactly five measured development comparisons; no tuning after confirmation starts.
- Serial, alternating variant order; C++17 O3 NDEBUG. No concurrent compiles or other solvers during measurements.
- Official tester runs all 1000 queries. Also validate all routes and independently recompute the official weighted score; cross-check visualizer.
- Save sources, source/tool/input/output hashes, per-seed results. Invalid or total tester+solver wall time over 2 seconds counts as zero (conservative timing).
- Common per-input best denominator includes all compared versions and any matching historical records.
- No third-party submission code is fetched or copied. Corpus summaries provide only structural inspiration.
