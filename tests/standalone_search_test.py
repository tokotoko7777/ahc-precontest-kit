#!/usr/bin/env python3
"""Check that selected practice files exactly inline the maintained examples."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    entries = [
        ("001", "ahc001_region_sa.cpp", ["batched-timer.hpp", "random.hpp", "axis-aligned-rectangle.hpp", "largest-empty-rectangle.hpp", "time-based-simulated-annealing.hpp", "scope-profiler.hpp"]),
        ("021", "ahc021_tree_beam.cpp", ["tree-beam-search.hpp", "radix-heap.hpp"]),
        ("032", "ahc032_action_beam.cpp", ["action-beam-search.hpp"]),
        ("058", "ahc058_prefix_sa.cpp", ["deterministic-rollout.hpp", "prefix-replay.hpp", "time-based-simulated-annealing.hpp"]),
        ("059", "ahc059_lns.cpp", ["large-neighborhood-search.hpp"]),
    ]
    for task, example, headers in entries:
        expected = (ROOT / "examples/search" / example).read_text()
        for header in headers:
            content = (ROOT / "library" / header).read_text().rstrip()
            marker = f'#include "../../library/{header}"'
            assert expected.count(marker) == 1, (example, header)
            expected = expected.replace(marker, f"// BEGIN LIBRARY: {header}\n{content}\n// END LIBRARY: {header}")
        actual = (ROOT / f"practice/ahc{task}/main.cpp").read_text()
        assert actual.rstrip() == expected.rstrip(), f"AHC{task}: standalone differs from example/header expansion"
    print(f"standalone search test passed: {len(entries)} examples")

if __name__ == "__main__": main()
