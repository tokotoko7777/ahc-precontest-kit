#!/usr/bin/env python3
"""Compare AHC026 solvers and independently replay every box operation."""

from __future__ import annotations

import argparse
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "ahc026-official"


@dataclass
class Result:
    score: int
    energy: int
    operations: int
    seconds: float = 0.0


def compile_solver(source: Path, output: Path, strict: bool) -> None:
    command = [
        "g++", "-std=c++17", "-O3", "-DNDEBUG",
        "-Wall", "-Wextra", "-pedantic",
    ]
    if strict:
        command += ["-Wshadow", "-Wconversion", "-Werror", "-I", str(ROOT)]
    subprocess.run(command + [str(source), "-o", str(output)], check=True)


def score_output(input_text: str, output_text: str) -> Result:
    tokens = list(map(int, input_text.split()))
    n, m = tokens[:2]
    height = n // m
    stacks = [tokens[2 + i * height:2 + (i + 1) * height]
              for i in range(m)]
    location = [-1] * (n + 1)
    for stack_id, stack in enumerate(stacks):
        for box in stack:
            location[box] = stack_id

    operations: list[tuple[int, int]] = []
    for line_number, line in enumerate(output_text.splitlines(), 1):
        fields = line.split()
        if len(fields) != 2:
            raise ValueError(f"line {line_number}: expected two integers")
        operations.append((int(fields[0]), int(fields[1])))
    if len(operations) > 5000:
        raise ValueError("output exceeds 5000 operations")

    next_target = 1
    energy = 0
    for turn, (box, destination) in enumerate(operations, 1):
        if not 1 <= box <= n or location[box] == -1:
            raise ValueError(f"turn {turn}: unavailable box {box}")
        source = location[box]
        if destination == 0:
            if box != next_target or stacks[source][-1] != box:
                raise ValueError(f"turn {turn}: illegal removal of {box}")
            stacks[source].pop()
            location[box] = -1
            next_target += 1
            continue

        target = destination - 1
        if not 0 <= target < m:
            raise ValueError(f"turn {turn}: invalid stack {destination}")
        first = stacks[source].index(box)
        moved = stacks[source][first:]
        energy += len(moved) + 1
        if target != source:
            del stacks[source][first:]
            stacks[target].extend(moved)
            for moved_box in moved:
                location[moved_box] = target

    if next_target != n + 1:
        raise ValueError(f"only removed boxes 1 through {next_target - 1}")
    return Result(max(1, 10000 - energy), energy, len(operations))


def run_solver(executable: Path, input_text: str) -> Result:
    started = time.perf_counter()
    completed = subprocess.run(
        [str(executable)], input=input_text, text=True,
        capture_output=True, timeout=5.0, check=True,
    )
    elapsed = time.perf_counter() - started
    result = score_output(input_text, completed.stdout)
    result.seconds = elapsed
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--inputs", type=Path, required=True,
        help="directory containing 0000.txt, 0001.txt, ... inputs",
    )
    parser.add_argument("--cases", type=int, default=10)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not 1 <= args.cases <= 10000:
        raise ValueError("--cases must be from 1 to 10000")

    BUILD.mkdir(parents=True, exist_ok=True)
    existing = BUILD / "existing"
    formatted = BUILD / "formatted"
    compile_solver(ROOT / "practice/ahc026/main.cpp", existing, strict=False)
    compile_solver(
        ROOT / "examples/search/ahc026_deterministic_rollout.cpp",
        formatted,
        strict=True,
    )

    old_results: list[Result] = []
    new_results: list[Result] = []
    for seed in range(args.cases):
        input_path = args.inputs / f"{seed:04d}.txt"
        if not input_path.is_file():
            raise FileNotFoundError(f"missing input: {input_path}")
        input_text = input_path.read_text()
        old = run_solver(existing, input_text)
        new = run_solver(formatted, input_text)
        old_results.append(old)
        new_results.append(new)
        print(
            f"seed={seed:04d} existing={old.score} formatted={new.score} "
            f"energy={old.energy}/{new.energy} "
            f"operations={old.operations}/{new.operations} "
            f"ms={1000 * old.seconds:.1f}/{1000 * new.seconds:.1f}"
        )

    old_sum = sum(result.score for result in old_results)
    new_sum = sum(result.score for result in new_results)
    wins = sum(new.score > old.score for old, new in zip(old_results, new_results))
    ties = sum(new.score == old.score for old, new in zip(old_results, new_results))
    losses = args.cases - wins - ties
    print(f"existing average={old_sum / args.cases:.2f} sum={old_sum}")
    print(
        f"formatted average={new_sum / args.cases:.2f} sum={new_sum} "
        f"win/tie/loss={wins}/{ties}/{losses}"
    )
    print(
        "average time ms="
        f"{1000 * sum(result.seconds for result in old_results) / args.cases:.1f}/"
        f"{1000 * sum(result.seconds for result in new_results) / args.cases:.1f} "
        "(existing/formatted)"
    )


if __name__ == "__main__":
    main()
