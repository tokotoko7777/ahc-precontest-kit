#!/usr/bin/env python3
"""Exercise exclusions with invented fixtures, never archived source code."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_source_policy import check, repository_paths


class SourcePolicyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.synthetic = b"invented fixture, no submission content\n"
        self.policy = {
            "removed_files": [{"path": "practice/ahc999/main.cpp",
                "sha256": hashlib.sha256(self.synthetic).hexdigest()},
                {"path": "benchmarks/retired_driver.py",
                 "sha256": hashlib.sha256(b"second invented fixture").hexdigest()}],
            "removed_practice": {"ahc999": ["ahc998"]},
            "excluded_submissions": [{"submission_id": "999999999"}],
            "blocked_component_symbols": ["invented_removed_component"],
            "blocked_external_references": ["https://example.invalid/denied"],
        }

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else data.encode())
        return name

    def findings(self, name, data):
        return check(self.root, self.policy, [self.write(name, data)])

    def test_renamed_copy_and_metadata_copy_are_detected(self):
        for name in ["library/renamed.hpp", "SOURCE_POLICY.md"]:
            self.assertTrue(self.findings(name, self.synthetic))

    def test_removed_case_cannot_return_under_another_filename(self):
        self.assertTrue(self.findings("practice/ahc999/new.cpp", "int main() {}"))

    def test_excluded_references_and_symbols_fail_without_printing_text(self):
        for data in ["https://atcoder.jp/contests/ahc999/submissions/999999999",
                     "https://example.invalid/denied", "invented_removed_component()",
                     "[old code](practice/ahc999/main.cpp)",
                     "from retired_driver import helper"]:
            issues = self.findings("new.md", data)
            self.assertTrue(issues)
            self.assertNotIn(data, repr(issues))

    def test_metadata_exception_is_exact_and_contest_is_not_blanket_banned(self):
        self.assertFalse(self.findings("SOURCE_POLICY.md", "999999999"))
        self.assertTrue(self.findings("notes/SOURCE_POLICY.md", "999999999"))
        self.assertFalse(self.findings("docs/independent.md", "AHC999: eligible submission 888888888"))
        self.assertFalse(self.findings("new.cpp", "invented_removed_component_other()"))

    def test_deleted_entries_are_skipped_and_untracked_files_checked(self):
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        self.write("tracked.md", "safe")
        subprocess.run(["git", "add", "tracked.md"], cwd=self.root, check=True)
        (self.root / "tracked.md").unlink()
        self.write("new.cpp", self.synthetic)
        self.write(".gitignore", "build/\n")
        self.write("build/ignored", self.synthetic)
        paths = repository_paths(self.root)
        self.assertNotIn("build/ignored", paths)
        issues = check(self.root, self.policy, paths)
        self.assertEqual([name for name, _ in issues], ["new.cpp"])


if __name__ == "__main__":
    unittest.main()
