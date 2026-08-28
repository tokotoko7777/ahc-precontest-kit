from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class BundleTest(unittest.TestCase):
    def test_bundle_is_single_cpp_file_and_compiles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_dir:
            temporary = Path(temporary_dir)
            bundled = temporary / "submission.cpp"
            binary = temporary / "submission"

            subprocess.run(
                [
                    "python3",
                    str(ROOT / "tools/bundle.py"),
                    str(ROOT / "examples/basic.cpp"),
                    "-o",
                    str(bundled),
                ],
                check=True,
            )

            content = bundled.read_text(encoding="utf-8")
            self.assertNotIn('#include "', content)
            self.assertEqual(content.count("class Timer {"), 1)
            self.assertEqual(content.count("class Random {"), 1)

            subprocess.run(
                [
                    "g++",
                    "-std=c++17",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-pedantic",
                    str(bundled),
                    "-o",
                    str(binary),
                ],
                check=True,
            )


if __name__ == "__main__":
    unittest.main()
