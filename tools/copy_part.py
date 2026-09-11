#!/usr/bin/env python3
"""Copy standalone library parts from one immutable commit or offline bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable, Optional
from urllib.parse import quote


DEFAULT_REPOSITORY_URL = "https://github.com/tokotoko7777/ahc-precontest-kit"
FULL_COMMIT_PATTERN = re.compile(r"[0-9a-f]{40}")
BUNDLE_FORMAT_VERSION = 1
LOCAL_INCLUDE_PATTERN = re.compile(
    rb'(?m)^[ \t]*#[ \t]*include[ \t]*"([^"\r\n]+)"'
    rb'[ \t]*(?://[^\r\n]*)?\r?\n?'
)


@dataclass(frozen=True)
class FrozenPart:
  path: str
  content: bytes
  sha256: str
  source_url: str


def run_git(root: Path, *arguments: str) -> bytes:
  completed = subprocess.run(
      ["git", "-C", str(root), *arguments],
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )
  if completed.returncode != 0:
    message = completed.stderr.decode("utf-8", errors="replace").strip()
    raise RuntimeError(message or "git command failed")
  return completed.stdout


def resolve_commit(root: Path, reference: str) -> str:
  commit = run_git(root, "rev-parse", "--verify", f"{reference}^{{commit}}")
  result = commit.decode("ascii").strip().lower()
  if not FULL_COMMIT_PATTERN.fullmatch(result):
    raise RuntimeError("git did not return a full commit SHA")
  return result


def normalize_repository_url(repository_url: str) -> str:
  result = repository_url.strip().rstrip("/")
  if result.endswith(".git"):
    result = result[:-4]
  if not result.startswith(("https://", "http://")):
    raise ValueError("repository URL must start with https:// or http://")
  return result


def normalize_part_path(raw_path: str) -> str:
  path = PurePosixPath(raw_path)
  if path.is_absolute() or ".." in path.parts or "." in path.parts:
    raise ValueError(f"unsafe part path: {raw_path}")
  normalized = path.as_posix()
  if normalized != raw_path.replace("\\", "/"):
    raise ValueError(f"part path must be normalized: {raw_path}")
  if len(path.parts) != 2 or path.parts[0] != "library":
    raise ValueError(f"part must be directly under library/: {raw_path}")
  if path.suffix != ".hpp":
    raise ValueError(f"part must be a .hpp file: {raw_path}")
  return normalized


def normalize_part_paths(raw_paths: Iterable[str]) -> list[str]:
  paths = [normalize_part_path(path) for path in raw_paths]
  if not paths:
    raise ValueError("at least one library/*.hpp part is required")
  if len(set(paths)) != len(paths):
    raise ValueError("the same part was specified more than once")
  return paths


def make_source_url(repository_url: str, commit: str, path: str) -> str:
  quoted_path = "/".join(quote(piece, safe="") for piece in path.split("/"))
  return f"{repository_url}/blob/{commit}/{quoted_path}"


def ensure_selected_paths_clean(root: Path, paths: list[str]) -> None:
  status = run_git(
      root, "status", "--porcelain=v1", "--untracked-files=all", "--", *paths
  ).decode("utf-8", errors="replace").strip()
  if status:
    raise RuntimeError(
        "selected parts have uncommitted changes; commit/stash them or use an "
        "already prepared offline bundle:\n" + status
    )


def load_parts_from_git(
    root: Path,
    reference: str,
    raw_paths: Iterable[str],
    repository_url: str,
) -> tuple[str, list[FrozenPart]]:
  paths = normalize_part_paths(raw_paths)
  ensure_selected_paths_clean(root, paths)
  commit = resolve_commit(root, reference)
  repository_url = normalize_repository_url(repository_url)
  parts = []
  for path in paths:
    object_name = f"{commit}:{path}"
    object_type = run_git(root, "cat-file", "-t", object_name).decode("ascii").strip()
    if object_type != "blob":
      raise RuntimeError(f"not a file in {commit}: {path}")
    content = run_git(root, "show", object_name)
    parts.append(
        FrozenPart(
            path=path,
            content=content,
            sha256=hashlib.sha256(content).hexdigest(),
            source_url=make_source_url(repository_url, commit, path),
        )
    )
  return commit, parts


def load_bundle_manifest(bundle: Path) -> dict:
  manifest_path = bundle / "manifest.json"
  try:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
  except (OSError, json.JSONDecodeError) as error:
    raise RuntimeError(f"cannot read {manifest_path}: {error}") from error
  if manifest.get("format_version") != BUNDLE_FORMAT_VERSION:
    raise RuntimeError("unsupported offline bundle format")
  commit = manifest.get("commit")
  if not isinstance(commit, str) or not FULL_COMMIT_PATTERN.fullmatch(commit):
    raise RuntimeError("offline manifest has no valid full commit SHA")
  if not isinstance(manifest.get("files"), dict):
    raise RuntimeError("offline manifest has no files index")
  normalize_repository_url(str(manifest.get("repository_url", "")))
  return manifest


def load_parts_from_bundle(
    bundle: Path, raw_paths: Iterable[str]
) -> tuple[str, list[FrozenPart]]:
  paths = normalize_part_paths(raw_paths)
  bundle = bundle.resolve()
  manifest = load_bundle_manifest(bundle)
  commit = manifest["commit"]
  repository_url = normalize_repository_url(manifest["repository_url"])
  parts = []
  for path in paths:
    entry = manifest["files"].get(path)
    if not isinstance(entry, dict):
      raise RuntimeError(f"part is not present in offline bundle: {path}")
    part_file = bundle / "parts" / Path(path)
    try:
      content = part_file.read_bytes()
    except OSError as error:
      raise RuntimeError(f"cannot read bundled part {path}: {error}") from error
    actual_sha256 = hashlib.sha256(content).hexdigest()
    expected_sha256 = entry.get("sha256")
    if actual_sha256 != expected_sha256:
      raise RuntimeError(f"SHA-256 mismatch for bundled part: {path}")
    source_url = make_source_url(repository_url, commit, path)
    if entry.get("source_url") != source_url:
      raise RuntimeError(f"source URL mismatch in manifest: {path}")
    parts.append(FrozenPart(path, content, actual_sha256, source_url))
  return commit, parts


def strip_selected_part_includes(
    main_source: bytes, selected_paths: Iterable[str]
) -> bytes:
  """Remove quoted includes for parts that were already copied above main."""
  selected = {path.encode("utf-8") for path in selected_paths}

  def replace_include(match: re.Match[bytes]) -> bytes:
    raw_path = match.group(1).replace(b"\\", b"/")
    pieces = [piece for piece in raw_path.split(b"/") if piece not in (b"", b".")]
    while pieces and pieces[0] == b"..":
      pieces.pop(0)
    normalized = b"/".join(pieces)
    return b"" if normalized in selected else match.group(0)

  return LOCAL_INCLUDE_PATTERN.sub(replace_include, main_source)


def render_parts(parts: Iterable[FrozenPart], main_source: Optional[bytes] = None) -> bytes:
  parts = tuple(parts)
  output = bytearray()
  for part in parts:
    output.extend(f"// BEGIN ahc-precontest-kit: {part.path}\n".encode())
    output.extend(f"// Source: {part.source_url}\n".encode())
    output.extend(f"// SHA-256: {part.sha256}\n".encode())
    output.extend(part.content)
    if part.content and not part.content.endswith(b"\n"):
      output.extend(b"\n")
    output.extend(f"// END ahc-precontest-kit: {part.path}\n\n".encode())
  if main_source is not None:
    main_source = strip_selected_part_includes(
        main_source, (part.path for part in parts)
    )
    output.extend(main_source)
    if main_source and not main_source.endswith(b"\n"):
      output.extend(b"\n")
  return bytes(output)


def write_output(output_path: Optional[Path], content: bytes) -> None:
  if output_path is None:
    sys.stdout.buffer.write(content)
    return
  output_path.parent.mkdir(parents=True, exist_ok=True)
  output_path.write_bytes(content)


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description=(
          "Copy standalone library/*.hpp files from exactly one Git commit, "
          "or from a verified offline bundle, and add immutable source URLs."
      )
  )
  source = parser.add_mutually_exclusive_group(required=True)
  source.add_argument("--ref", help="commit, tag, or ref to resolve to one commit")
  source.add_argument("--bundle", type=Path, help="prepared offline bundle directory")
  parser.add_argument("parts", nargs="+", help="library/*.hpp files in copy order")
  parser.add_argument("--main", type=Path, help="problem-specific main.cpp to append")
  parser.add_argument("-o", "--output", type=Path, help="output file (default: stdout)")
  parser.add_argument(
      "--repository-url",
      default=DEFAULT_REPOSITORY_URL,
      help="public repository URL used with --ref",
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
  if args.main is not None and args.output is not None:
    if args.main.resolve() == args.output.resolve():
      raise ValueError("output must differ from --main")

  if args.output is not None:
    output = args.output.resolve()
    if output == Path(__file__).resolve():
      raise ValueError("output must not overwrite copy_part.py")
    if args.ref is not None:
      source_root = args.repo_root.resolve()
    else:
      source_root = args.bundle.resolve() / "parts"
    selected_sources = {
        (source_root / Path(normalize_part_path(path))).resolve()
        for path in args.parts
    }
    if output in selected_sources:
      raise ValueError("output must not overwrite a selected source part")

  if args.ref is not None:
    commit, parts = load_parts_from_git(
        args.repo_root.resolve(), args.ref, args.parts, args.repository_url
    )
  else:
    commit, parts = load_parts_from_bundle(args.bundle, args.parts)

  main_source = args.main.read_bytes() if args.main is not None else None
  write_output(args.output, render_parts(parts, main_source))
  destination = str(args.output) if args.output is not None else "stdout"
  print(
      f"copied {len(parts)} part(s) from immutable commit {commit} to {destination}",
      file=sys.stderr,
  )
  return 0


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except (OSError, RuntimeError, ValueError) as error:
    print(f"error: {error}", file=sys.stderr)
    raise SystemExit(2)
