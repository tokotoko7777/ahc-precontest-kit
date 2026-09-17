#!/usr/bin/env python3
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from ahc071_key_benchmark import summarize
from ahc071_official_benchmark import ported_score


class ScoreTest(unittest.TestCase):
    def test_common_best_includes_repeats_and_excludes_overtime(self):
        rows = []
        for name, scores, over in [("old", [50, 80], [0, 0]), ("new", [100, 9999], [0, 1])]:
            for repeat, (score, invalid) in enumerate(zip(scores, over)):
                rows.append(dict(version=name, seed=0, repeat=repeat, score=score,
                                 seconds=1, over_2s=invalid, beams=0, rebuilds=0))
        output = io.StringIO()
        with contextlib.redirect_stdout(output): summarize(rows)
        lines = output.getvalue().splitlines()
        self.assertIn("common_best=130.000000/200", lines[0])
        self.assertIn("average_best_ratio=65.000000%", lines[0])
        self.assertIn("common_best=100.000000/200", lines[1])

    def test_replay(self):
        with tempfile.TemporaryDirectory() as directory:
            inp, out = Path(directory) / "input.txt", Path(directory) / "output.txt"
            inp.write_text("3 2 1\n5 6 9 13 17\n1 1\n")
            out.write_text("2\n0 0 3\n1 1 1\n")
            self.assertEqual(ported_score(inp, out), 20)
            for invalid in ["1\n1 1 1\n", "1\n0 0 3\n", "2\n0 0 3\n1 0 1\n", "1\n0 0 2\n", "1\n2 0 3\n"]:
                out.write_text(invalid)
                with self.assertRaises(RuntimeError): ported_score(inp, out)


if __name__ == "__main__": unittest.main()
