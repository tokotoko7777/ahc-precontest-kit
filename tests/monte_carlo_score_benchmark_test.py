#!/usr/bin/env python3
"""Independent AHC015 replay checks; no solver execution."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from ahc015_coalesced_benchmark import replay_score

values = [1] * 100 + [1] * 100
for direction in ("F", "B", "L", "R"):
    assert replay_score(values, [direction] * 100) == 1000000
for invalid in ([], ["F"] * 99, ["FB"] * 100, ["X"] * 100):
    try:
        replay_score(values, invalid)
        raise AssertionError("accepted illegal directions")
    except ValueError:
        pass
for invalid in ([], [0] * 100 + [1] * 100, [1] * 100 + [101] * 100):
    try:
        replay_score(invalid, ["F"] * 100)
        raise AssertionError("accepted illegal input")
    except ValueError:
        pass
print("Monte Carlo score replay tests passed")
