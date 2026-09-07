#!/usr/bin/env python3

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import make_offline_bundle  # noqa: E402


def checked_run(arguments: list[str], **kwargs) -> subprocess.CompletedProcess:
  completed = subprocess.run(
      arguments,
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      **kwargs,
  )
  if completed.returncode != 0:
    raise RuntimeError(
        f"command failed: {arguments}\n"
        + completed.stdout.decode("utf-8", errors="replace")
        + completed.stderr.decode("utf-8", errors="replace")
    )
  return completed


def main() -> None:
  compiler = shutil.which(os.environ.get("CXX", "g++"))
  if compiler is None:
    raise RuntimeError("C++ compiler was not found")

  with tempfile.TemporaryDirectory(prefix="repository-bundle-test-") as directory:
    temp = Path(directory)
    bundle = temp / "kit"
    commit, count = make_offline_bundle.create_bundle(ROOT, "HEAD", bundle)
    if count < 3:
      raise AssertionError("repository bundle contains too few parts")

    generated = temp / "submission.cpp"
    offline_environment = os.environ.copy()
    empty_path = temp / "empty-path"
    empty_path.mkdir()
    offline_environment["PATH"] = str(empty_path)
    checked_run(
        [
            sys.executable,
            str(bundle / "copy_part.py"),
            "--bundle",
            str(bundle),
            "--main",
            str(ROOT / "tests" / "fixtures" / "copied_parts_main.cpp"),
            "library/timer.hpp",
            "library/random.hpp",
            "library/simulated-annealing.hpp",
            "-o",
            str(generated),
        ],
        env=offline_environment,
    )
    source = generated.read_text(encoding="utf-8")
    if source.count(f"/blob/{commit}/") != 3:
      raise AssertionError("generated source does not contain three fixed URLs")
    if '#include "library/' in source:
      raise AssertionError("generated source still depends on a local kit header")

    binary = temp / "submission"
    checked_run(
        [
            compiler,
            "-std=c++17",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            str(generated),
            "-o",
            str(binary),
        ]
    )
    checked_run([str(binary)])
  print("repository offline bundle test passed")


if __name__ == "__main__":
  main()
