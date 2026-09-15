#!/usr/bin/env python3
"""Check score replay and time-valid common denominators without running a solver."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "benchmarks"))
from ahc032_core_benchmark import MOD, replay_score, summarize


with tempfile.TemporaryDirectory(prefix="beam-score-test-") as directory:
    work = Path(directory)
    data, answer = work / "in.txt", work / "out.txt"
    data.write_text("9 20 81\n" + " ".join(map(str, [MOD - 1] * 81 + [2] * 180)))
    answer.write_text("0\n")
    assert replay_score(data, answer) == 81 * (MOD - 1)
    answer.write_text("1\n19 6 6\n")
    assert replay_score(data, answer) == 72 * (MOD - 1) + 9
    for invalid in ["", "-1", "82", "1\n20 0 0", "1\n0 7 0", "1\n0 0 -1", "0 1"]:
        answer.write_text(invalid)
        try:
            replay_score(data, answer)
            raise AssertionError("invalid output accepted")
        except ValueError:
            pass

rows = [dict(seed=0, repeat=0, version="base", score=80, seconds=1, over_2s=0),
        dict(seed=0, repeat=0, version="new", score=100, seconds=1, over_2s=0),
        dict(seed=0, repeat=0, version="too_slow", score=1000, seconds=3, over_2s=1)]
output = io.StringIO()
with contextlib.redirect_stdout(output):
    summarize(rows)
report = output.getvalue()
assert "common_best=80.000000/100" in report
assert "common_best=100.000000/100" in report
assert "common_best=0.000000/100" in report
print("beam score benchmark tests passed")
