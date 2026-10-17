#!/usr/bin/env python3
"""Detect known excluded sources without printing their contents or using HTTP."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


METADATA_PATHS = frozenset({
    "config/source_policy_exclusions.json",
    "SOURCE_POLICY.md",
    "docs/technique-guide/detailed/SOURCES.md",
})


def check(root: Path, policy: dict, paths: list[str]) -> list[tuple[str, str]]:
    """Return path/reason pairs only. Metadata exceptions never bypass hashes."""
    issues = []
    removed = {row["path"]: row for row in policy["removed_files"]}
    hashes = {row["sha256"] for row in removed.values()}
    prefixes = tuple(f"practice/{contest}/" for contest in policy["removed_practice"])
    ids = {str(row["submission_id"]) for row in policy["excluded_submissions"]}
    symbols = [*policy["blocked_component_symbols"],
               *(Path(name).stem for name in removed if name.endswith(".py"))]
    references = policy["blocked_external_references"]
    for name in sorted(set(paths)):
        path = root / name
        if not path.exists():
            continue  # Deleted files may still be in the index before committing.
        if name in removed or (prefixes and name.startswith(prefixes)):
            issues.append((name, "removed source path reintroduced"))
        if not path.is_file():
            continue
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() in hashes:
            issues.append((name, "removed file content reintroduced (SHA-256)"))
        if name in METADATA_PATHS:
            continue
        try:
            content = data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        if ids.intersection(re.findall(r"(?<![0-9])[0-9]{7,}(?![0-9])", content)):
            issues.append((name, "excluded submission reference"))
        if any(reference in content for reference in references):
            issues.append((name, "excluded external reference"))
        if any(re.search(r"(?<!\w)" + re.escape(symbol) + r"(?!\w)", content)
               for symbol in symbols):
            issues.append((name, "removed component symbol"))
        if any(name_removed in content for name_removed in removed):
            issues.append((name, "reference to removed file"))
    return issues


def repository_paths(root: Path) -> list[str]:
    raw = subprocess.check_output([
        "git", "ls-files", "-z", "--cached", "--others", "--exclude-standard",
    ], cwd=root)
    return [name.decode("utf-8") for name in raw.split(b"\0") if name]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    policy = json.loads((root / "config/source_policy_exclusions.json").read_text())
    paths = repository_paths(root)
    issues = check(root, policy, paths)
    for name, reason in issues:
        print(f"{name}: {reason}")
    if issues:
        print(f"Source policy failed: {len(issues)} finding(s); source text was not printed.")
        return 1
    print(f"Source policy passed: {len(paths)} indexed/unignored paths checked.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
