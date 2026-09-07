#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess


def main() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument("binary")
  parser.add_argument("expected_difference")
  args = parser.parse_args()

  completed = subprocess.run(
      [args.binary],
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      text=True,
      encoding="utf-8",
  )
  output = completed.stdout + completed.stderr
  if completed.returncode == 0:
    raise AssertionError(f"fault fixture unexpectedly succeeded: {args.binary}")
  required = [
      "seed=",
      "iteration=",
      "moves=",
      f"first_difference={args.expected_difference}",
  ]
  missing = [value for value in required if value not in output]
  if missing:
    raise AssertionError(
        f"fault fixture log is missing {missing}: {output.rstrip()}"
    )
  print(
      f"debug fixture detected {args.expected_difference}: "
      f"{output.strip()}"
  )


if __name__ == "__main__":
  main()
