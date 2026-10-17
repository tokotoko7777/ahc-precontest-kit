"""Process and scoring utilities for the retained noninteractive benchmarks.

Implemented from their call contracts and the official visualizer interface;
contains no solver, contest strategy, or archived rollout implementation.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    kwargs.setdefault("timeout", 30)
    result = subprocess.run(command, check=True, **kwargs)
    return result.stdout, result.stderr


def official_score(contest, tool, solver, input_path, work, label):
    if contest not in {"021", "032", "058"}:
        raise ValueError("unsupported noninteractive benchmark")
    tool, solver, input_path, work = (
        Path(path).resolve() for path in (tool, solver, input_path, work)
    )
    output_path = work / f"{input_path.stem}-{label}.out"
    with input_path.open("rb") as source, output_path.open("wb") as destination:
        started = time.perf_counter()
        run([str(solver)], stdin=source, stdout=destination,
            stderr=subprocess.PIPE, cwd=work)
        seconds = time.perf_counter() - started
    stdout, stderr = run([str(tool), str(input_path), str(output_path)],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True, cwd=work)
    # Official tools can report an invalid answer while exiting successfully.
    # Accept exactly one score and no accompanying error text.
    match = re.fullmatch(r"\s*(?:Score\s*=\s*)?([0-9]+)\s*", stdout + "\n" + stderr)
    if match is None:
        raise ValueError("official visualizer did not return a clean integer score")
    return int(match[1]), seconds, hashlib.sha256(output_path.read_bytes()).hexdigest()
