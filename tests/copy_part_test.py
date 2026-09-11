#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import copy_part  # noqa: E402
import make_offline_bundle  # noqa: E402


def run(*arguments: str, cwd: Path) -> subprocess.CompletedProcess[bytes]:
  return subprocess.run(
      list(arguments),
      cwd=cwd,
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )


def require_failure(action, expected_text: str) -> None:
  try:
    action()
  except (RuntimeError, ValueError) as error:
    assert expected_text in str(error), (expected_text, str(error))
  else:
    raise AssertionError(f"expected failure containing: {expected_text}")


def commit_all(repository: Path, message: str) -> str:
  assert run("git", "add", ".", cwd=repository).returncode == 0
  result = run("git", "commit", "-q", "-m", message, cwd=repository)
  assert result.returncode == 0, result.stderr.decode()
  return run("git", "rev-parse", "HEAD", cwd=repository).stdout.decode().strip()


def make_repository(directory: Path) -> tuple[Path, str, bytes, bytes]:
  repository = directory / "repository"
  (repository / "library").mkdir(parents=True)
  (repository / "tools").mkdir()
  first = b"#include <vector>\n\nstruct FirstPart { int value = 1; };\n"
  second = b"#include <string>\n\nstruct SecondPart { int value = 2; };\n"
  (repository / "library" / "first.hpp").write_bytes(first)
  (repository / "library" / "second.hpp").write_bytes(second)
  (repository / "tools" / "copy_part.py").write_bytes(
      (ROOT / "tools" / "copy_part.py").read_bytes()
  )
  (repository / "tools" / "make_offline_bundle.py").write_bytes(
      (ROOT / "tools" / "make_offline_bundle.py").read_bytes()
  )
  (repository / "README.md").write_text("# fixture\n", encoding="utf-8")
  (repository / "PRECONTEST.md").write_text("fixture docs\n", encoding="utf-8")
  assert run("git", "init", "-q", cwd=repository).returncode == 0
  assert run("git", "config", "user.email", "test@example.com", cwd=repository).returncode == 0
  assert run("git", "config", "user.name", "Test", cwd=repository).returncode == 0
  commit = commit_all(repository, "fixture")
  return repository, commit, first, second


def test_git_copy_and_fixed_urls(temp: Path) -> None:
  repository, commit, first, second = make_repository(temp)
  resolved, parts = copy_part.load_parts_from_git(
      repository,
      "HEAD",
      ["library/first.hpp", "library/second.hpp"],
      "https://github.com/example/kit.git",
  )
  assert resolved == commit
  assert [part.content for part in parts] == [first, second]
  for part in parts:
    assert part.source_url == (
        f"https://github.com/example/kit/blob/{commit}/{part.path}"
    )

  main_source = (
      b'#include "../../library/first.hpp"\n'
      b'# include "library/second.hpp" // bundled above\n'
      b'#include "../library/not-selected.hpp"\n'
      b"int main() { return FirstPart{}.value + SecondPart{}.value - 3; }\n"
  )
  rendered = copy_part.render_parts(parts, main_source)
  assert b'#include "../../library/first.hpp"' not in rendered
  assert b'# include "library/second.hpp"' not in rendered
  assert b'#include "../library/not-selected.hpp"' in rendered
  assert b"int main()" in rendered
  assert rendered.count(b"// Source:") == 2
  assert rendered.count(commit.encode()) == 2
  for part in parts:
    assert part.content in rendered

  # A clean newer checkout may intentionally copy an older fixed commit. Both
  # parts must still come from that one commit rather than the working tree.
  (repository / "library" / "first.hpp").write_text(
      "struct FirstPart { int value = 99; };\n", encoding="utf-8"
  )
  newer_commit = commit_all(repository, "newer fixture")
  assert newer_commit != commit
  old_commit, old_parts = copy_part.load_parts_from_git(
      repository,
      commit,
      ["library/first.hpp", "library/second.hpp"],
      "https://github.com/example/kit",
  )
  assert old_commit == commit
  assert [part.content for part in old_parts] == [first, second]

  (repository / "library" / "first.hpp").write_text(
      "struct FirstPart { int value = -1; };\n", encoding="utf-8"
  )
  require_failure(
      lambda: copy_part.load_parts_from_git(
          repository,
          commit,
          ["library/first.hpp"],
          "https://github.com/example/kit",
      ),
      "uncommitted changes",
  )


def test_offline_bundle(temp: Path) -> None:
  repository, commit, first, second = make_repository(temp)
  bundle = temp / "offline"
  bundled_commit, part_count = make_offline_bundle.create_bundle(
      repository, commit, bundle, "https://github.com/example/kit"
  )
  assert bundled_commit == commit
  assert part_count == 2
  manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
  assert manifest["commit"] == commit
  assert set(manifest["files"]) == {
      "library/first.hpp",
      "library/second.hpp",
  }
  assert (bundle / "parts" / "library" / "first.hpp").read_bytes() == first
  assert (bundle / "parts" / "library" / "second.hpp").read_bytes() == second

  main_source = temp / "main.cpp"
  main_source.write_text("int main() { return FirstPart{}.value - 1; }\n", encoding="utf-8")
  output = temp / "submission.cpp"
  result = run(
      sys.executable,
      "copy_part.py",
      "--bundle",
      ".",
      "--main",
      str(main_source),
      "library/first.hpp",
      "-o",
      str(output),
      cwd=bundle,
  )
  assert result.returncode == 0, result.stderr.decode()
  generated = output.read_bytes()
  assert first in generated
  assert commit.encode() in generated
  assert b"int main()" in generated

  (bundle / "parts" / "library" / "first.hpp").write_bytes(b"tampered\n")
  require_failure(
      lambda: copy_part.load_parts_from_bundle(bundle, ["library/first.hpp"]),
      "SHA-256 mismatch",
  )

  (repository / "library" / "uncommitted.hpp").write_text(
      "struct Uncommitted {};\n", encoding="utf-8"
  )
  require_failure(
      lambda: make_offline_bundle.create_bundle(
          repository,
          commit,
          temp / "must-not-exist",
          "https://github.com/example/kit",
      ),
      "uncommitted changes",
  )


def test_rejected_inputs() -> None:
  require_failure(
      lambda: copy_part.normalize_part_paths(["../library/a.hpp"]),
      "unsafe part path",
  )
  require_failure(
      lambda: copy_part.normalize_part_paths(["practice/a.hpp"]),
      "directly under library",
  )
  require_failure(
      lambda: copy_part.normalize_part_paths(
          ["library/a.hpp", "library/a.hpp"]
      ),
    "more than once",
  )

  main_source = (
      b'#include "./library/a.hpp"\r\n'
      b'#include "..\\..\\library\\b.hpp"\n'
      b'#include <library/a.hpp>\n'
  )
  stripped = copy_part.strip_selected_part_includes(
      main_source, ["library/a.hpp", "library/b.hpp"]
  )
  assert stripped == b'#include <library/a.hpp>\n'


def main() -> None:
  with tempfile.TemporaryDirectory(prefix="copy-part-test-") as directory:
    test_git_copy_and_fixed_urls(Path(directory) / "git")
  with tempfile.TemporaryDirectory(prefix="copy-part-test-") as directory:
    test_offline_bundle(Path(directory))
  test_rejected_inputs()
  print("copy_part tests passed")


if __name__ == "__main__":
  main()
