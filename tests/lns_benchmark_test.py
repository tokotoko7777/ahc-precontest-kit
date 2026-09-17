#!/usr/bin/env python3
"""Independent AHC059 replay and common-denominator regression tests."""
import contextlib
import io
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from lns_official_benchmark import score_059, summary, valid


class LnsBenchmarkTest(unittest.TestCase):
    def test_replay(self):
        board = "20\n" + " ".join(str(i // 2) for i in range(400))
        commands = []
        for row in range(20):
            if row: commands.append("D")
            for col in range(20):
                if col: commands.append("R" if row % 2 == 0 else "L")
                commands.append("Z")
        self.assertEqual(score_059(board, "\n".join(commands)), (16001, 399))
        self.assertEqual(score_059(board, ""), (0, 0))
        for bad in ("U", "L", "ZZ", "Z\nZ", "X", "Q", "R\n" * 16001):
            with self.assertRaises(ValueError): score_059(board, bad)
        # 置く操作も採点する。Z Xで元に戻り、何も消していない。
        self.assertEqual(score_059(board, "Z\nX"), (0, 0))

    def test_shared_best(self):
        def row(version, score, **extra):
            return dict(version=version, score=score, input_sha256="same", legal=1,
                        over_2s=0, seconds=1, **extra)
        rows = [row("a", 80), row("a", 100), row("b", 90), row("b", 95)]
        history = [row("old", 200), row("diagnostic", 10000, diagnostic=1)]
        capture = io.StringIO()
        with contextlib.redirect_stdout(capture): summary(rows, history)
        self.assertIn("a: mean_score=90.000 relative=90.000000/200", capture.getvalue())
        self.assertIn("b: mean_score=92.500 relative=92.500000/200", capture.getvalue())
        self.assertFalse(valid(dict(legal=1, over_2s=1)))
        self.assertFalse(valid(dict(legal=0, over_2s=0)))
        self.assertFalse(valid(history[-1]))


if __name__ == "__main__": unittest.main()
