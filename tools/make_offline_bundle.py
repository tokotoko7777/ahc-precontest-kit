#!/usr/bin/env python3
"""Create a network-free, hash-checked copy of one published kit commit."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from copy_part import (
    BUNDLE_FORMAT_VERSION,
    DEFAULT_REPOSITORY_URL,
    ensure_selected_paths_clean,
    make_source_url,
    normalize_part_paths,
    normalize_repository_url,
    resolve_commit,
    run_git,
)


OPTIONAL_DOCUMENTS = [
    "README.md",
    "USAGE.md",
    "ALGORITHM_SELECTION.md",
    "SEARCH_GUIDE.md",
    "PERFORMANCE.md",
    "PRECONTEST.md",
]
REQUIRED_TOOL = "tools/copy_part.py"
REQUIRED_GENERATOR = "tools/make_offline_bundle.py"


def object_exists(root: Path, commit: str, path: str) -> bool:
  completed = subprocess.run(
      ["git", "-C", str(root), "cat-file", "-e", f"{commit}:{path}"],
      check=False,
      stdout=subprocess.DEVNULL,
      stderr=subprocess.DEVNULL,
  )
  return completed.returncode == 0


def list_library_parts(root: Path, commit: str) -> list[str]:
  output = run_git(
      root, "ls-tree", "-r", "--name-only", commit, "--", "library"
  ).decode("utf-8")
  candidates = [line for line in output.splitlines() if line.endswith(".hpp")]
  return normalize_part_paths(candidates)


def load_committed_file(root: Path, commit: str, path: str) -> bytes:
  return run_git(root, "show", f"{commit}:{path}")


def create_bundle(
    root: Path,
    reference: str,
    output: Path,
    repository_url: str = DEFAULT_REPOSITORY_URL,
) -> tuple[str, int]:
  root = root.resolve()
  output = output.resolve()
  if output.exists():
    raise ValueError(f"output already exists: {output}")
  output.parent.mkdir(parents=True, exist_ok=True)

  commit = resolve_commit(root, reference)
  repository_url = normalize_repository_url(repository_url)
  part_paths = list_library_parts(root, commit)
  required_support = [REQUIRED_TOOL, REQUIRED_GENERATOR]
  missing_support = [
      path for path in required_support if not object_exists(root, commit, path)
  ]
  if missing_support:
    raise RuntimeError(
        f"{', '.join(missing_support)} is not stored in {commit}; "
        "publish the tools first"
    )
  tracked_support = [
      path for path in [*required_support, *OPTIONAL_DOCUMENTS]
      if object_exists(root, commit, path)
  ]
  # directory指定にして、commitにまだ存在しない未追跡の新規.hppも見落とさない。
  ensure_selected_paths_clean(root, ["library", *tracked_support])

  with tempfile.TemporaryDirectory(prefix="ahc-kit-bundle-", dir=output.parent) as temp:
    staging = Path(temp) / output.name
    (staging / "parts" / "library").mkdir(parents=True)

    files = {}
    for path in part_paths:
      content = load_committed_file(root, commit, path)
      destination = staging / "parts" / path
      destination.write_bytes(content)
      files[path] = {
          "sha256": hashlib.sha256(content).hexdigest(),
          "source_url": make_source_url(repository_url, commit, path),
      }

    copied_tool = load_committed_file(root, commit, REQUIRED_TOOL)
    (staging / "copy_part.py").write_bytes(copied_tool)
    generator = load_committed_file(root, commit, REQUIRED_GENERATOR)

    included_files = {}
    for path in OPTIONAL_DOCUMENTS:
      if not object_exists(root, commit, path):
        continue
      content = load_committed_file(root, commit, path)
      destination = staging / "docs" / path
      destination.parent.mkdir(parents=True, exist_ok=True)
      destination.write_bytes(content)
      included_files[path] = hashlib.sha256(content).hexdigest()

    manifest = {
        "format_version": BUNDLE_FORMAT_VERSION,
        "repository_url": repository_url,
        "commit": commit,
        "files": files,
        "included_files": included_files,
        "copy_tool_sha256": hashlib.sha256(copied_tool).hexdigest(),
        "bundle_tool_sha256": hashlib.sha256(generator).hexdigest(),
    }
    (staging / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (staging / "README.txt").write_text(
        "ahc-precontest-kit offline bundle\n"
        f"commit: {commit}\n\n"
        "Copy parts without Git or network access:\n"
        "  python3 copy_part.py --bundle . library/timer.hpp -o copied.hpp\n\n"
        "Append your main.cpp in the same operation:\n"
        "  python3 copy_part.py --bundle . --main main.cpp "
        "library/timer.hpp -o submission.cpp\n\n"
        "Every part is checked against manifest.json before it is copied.\n",
        encoding="utf-8",
    )
    staging.rename(output)

  return commit, len(part_paths)


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description="Save one immutable kit commit for use without network access."
  )
  parser.add_argument("--ref", required=True, help="published commit or tag")
  parser.add_argument("-o", "--output", required=True, type=Path)
  parser.add_argument(
      "--repository-url",
      default=DEFAULT_REPOSITORY_URL,
      help="public repository URL recorded in the manifest",
  )
  parser.add_argument(
      "--repo-root",
      type=Path,
      default=Path(__file__).resolve().parents[1],
      help=argparse.SUPPRESS,
  )
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  commit, count = create_bundle(
      args.repo_root, args.ref, args.output, args.repository_url
  )
  print(f"saved {count} part(s) from immutable commit {commit} to {args.output}")
  return 0


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except (OSError, RuntimeError, ValueError) as error:
    print(f"error: {error}", file=sys.stderr)
    raise SystemExit(2)
