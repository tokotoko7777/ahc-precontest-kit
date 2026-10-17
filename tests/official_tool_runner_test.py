"""Test process/score handling using tiny invented programs, not AHC solutions."""
import hashlib
import importlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from official_tool_runner import official_score, run


class OfficialToolRunnerTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.input = self.work / "0012.txt"
        self.input.write_text("fixture\n")
        self.solver = self.program("solver", "print('answer')")

    def program(self, name, body):
        path = self.work / name
        path.write_text(f"#!{sys.executable}\n{body}\n")
        path.chmod(0o755)
        return path

    def score(self, body, contest="032"):
        return official_score(contest, self.program("vis", body), self.solver,
                              self.input, self.work, "candidate")

    def test_scores_output_location_and_digest(self):
        for contest in ["021", "032", "058"]:
            for body in ["print('Score = 42')", "print('42')",
                         "import sys; print('Score = 42', file=sys.stderr)"]:
                score, seconds, digest = self.score(body, contest)
                self.assertEqual(score, 42)
                self.assertGreaterEqual(seconds, 0)
                self.assertEqual((self.work / "0012-candidate.out").read_bytes(), b"answer\n")
                self.assertEqual(digest, hashlib.sha256(b"answer\n").hexdigest())
        self.assertEqual(self.score("print('Score = 0')")[0], 0)

    def test_invalid_answer_cannot_be_logged_as_success(self):
        for body in ["print('invalid answer\\nScore = 0')", "print('Score = 1\\nScore = 2')",
                     "print('Score = -1')", "pass",
                     "import sys; print('Score = 42'); print('error', file=sys.stderr)"]:
            with self.assertRaises(ValueError):
                self.score(body)

    def test_process_failure_is_not_scored(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.score("raise SystemExit(2)")
        self.solver = self.program("failed_solver", "raise SystemExit(3)")
        with self.assertRaises(subprocess.CalledProcessError):
            self.score("print('Score = 42')")

    def test_run_contract_and_benchmark_imports(self):
        out, err = run([sys.executable, "-c", "print('ok')"],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual((out, err), ("ok\n", ""))
        for module in ["ahc001_official_benchmark", "ahc021_core_benchmark",
                       "ahc032_core_benchmark", "ahc032_official_benchmark",
                       "ahc058_annealing_benchmark"]:
            importlib.import_module(module)


if __name__ == "__main__":
    unittest.main()
