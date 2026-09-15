#!/usr/bin/env python3
"""Validate independent AHC021 replay without running a solver."""
from pathlib import Path
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from ahc021_core_benchmark import replay_score

with tempfile.TemporaryDirectory() as directory:
    data, answer = Path(directory) / "in", Path(directory) / "out"
    data.write_text(" ".join(map(str, range(465))))
    answer.write_text("0")
    assert replay_score(data, answer) == 100000
    answer.write_text("1 0 0 1 0")
    assert replay_score(data, answer) == 49950
    answer.write_text("2 0 0 1 0 1 0 0 0")
    assert replay_score(data, answer) == 99990
    for invalid in ["", "-1", "10001", "1 0 0 2 0", "1 0 0 0 0", "1 29 0 30 0", "1 1 2 1 1", "0 1"]:
        answer.write_text(invalid)
        try:
            replay_score(data, answer)
            raise AssertionError("accepted illegal answer")
        except ValueError:
            pass
    data.write_text(" ".join(["0"] * 465))
    answer.write_text("0")
    try:
        replay_score(data, answer)
        raise AssertionError("accepted non-permutation input")
    except ValueError:
        pass
print("tree score replay tests passed")
