#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LIBRARY = ROOT / "library"
MARKER = "// Pre-contest public source (created with generative AI):"
URL_PREFIX = (
    "// https://github.com/tokotoko7777/"
    "ahc-precontest-kit/blob/main/library/"
)


def main() -> None:
  headers = sorted(LIBRARY.glob("*.hpp"))
  if not headers:
    raise AssertionError("no library headers found")

  for header in headers:
    lines = header.read_text(encoding="utf-8").splitlines()
    expected_url = URL_PREFIX + header.name
    if lines.count(MARKER) != 1:
      raise AssertionError(f"{header.name}: source marker must appear once")
    if lines.count(expected_url) != 1:
      raise AssertionError(f"{header.name}: public source URL is missing")

    marker_line = lines.index(MARKER)
    if marker_line + 1 >= len(lines) or lines[marker_line + 1] != expected_url:
      raise AssertionError(f"{header.name}: URL must follow its marker")

    include_lines = [
        index for index, line in enumerate(lines) if line.startswith("#include ")
    ]
    if include_lines:
      if marker_line != include_lines[-1] + 1:
        raise AssertionError(
            f"{header.name}: source URL must be immediately after includes"
        )
    elif marker_line != 0:
      raise AssertionError(
          f"{header.name}: a header without includes must start with the URL"
      )

  print(f"source URL test passed for {len(headers)} headers")


if __name__ == "__main__":
  main()
