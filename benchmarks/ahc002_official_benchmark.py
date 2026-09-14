#!/usr/bin/env python3
"""Compare the formatted AHC002 solver with the existing solver.

The input directory must contain the official local inputs named 0000.txt,
0001.txt, ... .  This script validates every command and computes the official
per-case score without requiring the visualizer.
"""

from __future__ import annotations

import argparse
import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "ahc002-official"


@dataclass
class Result:
    score: int
    moves: int


def compile_solver(source: Path, output: Path, strict: bool) -> None:
    flags = [
        "g++",
        "-std=c++17",
        "-O3",
        "-DNDEBUG",
        "-Wall",
        "-Wextra",
        "-pedantic",
    ]
    if strict:
        flags += ["-Wshadow", "-Wconversion", "-Werror", "-I", str(ROOT)]
    subprocess.run(flags + [str(source), "-o", str(output)], check=True)


def score_output(input_text: str, output_text: str) -> Result:
    values = list(map(int, input_text.split()))
    if len(values) != 5002:
        raise ValueError(f"expected 5002 input integers, got {len(values)}")
    row, column = values[0], values[1]
    tile = values[2:2502]
    point = values[2502:5002]
    commands = output_text.strip()
    if any(character not in "UDLR" for character in commands):
        raise ValueError("output contains a character other than U/D/L/R")

    used = {tile[row * 50 + column]}
    score = point[row * 50 + column]
    direction = {
        "U": (-1, 0),
        "D": (1, 0),
        "L": (0, -1),
        "R": (0, 1),
    }
    for turn, command in enumerate(commands, 1):
        delta_row, delta_column = direction[command]
        row += delta_row
        column += delta_column
        if not (0 <= row < 50 and 0 <= column < 50):
            raise ValueError(f"turn {turn}: moved outside the board")
        tile_id = tile[row * 50 + column]
        if tile_id in used:
            raise ValueError(f"turn {turn}: visited tile {tile_id} twice")
        used.add(tile_id)
        score += point[row * 50 + column]
    return Result(score, len(commands))


def run_solver(executable: Path, input_text: str) -> Result:
    completed = subprocess.run(
        [str(executable)],
        input=input_text,
        text=True,
        capture_output=True,
        timeout=4.0,
        check=True,
    )
    return score_output(input_text, completed.stdout)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--inputs",
        type=Path,
        required=True,
        help="directory containing official 0000.txt, 0001.txt, ... inputs",
    )
    parser.add_argument("--cases", type=int, default=10)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not 1 <= args.cases <= 100:
        raise ValueError("--cases must be from 1 to 100")

    BUILD.mkdir(parents=True, exist_ok=True)
    existing = BUILD / "existing"
    formatted = BUILD / "formatted"
    compile_solver(ROOT / "practice/ahc002/main.cpp", existing, strict=False)
    compile_solver(
        ROOT / "examples/search/ahc002_destroy_repair_sa.cpp",
        formatted,
        strict=True,
    )

    existing_results: list[Result] = []
    formatted_results: list[Result] = []
    for seed in range(args.cases):
        input_path = args.inputs / f"{seed:04d}.txt"
        if not input_path.is_file():
            raise FileNotFoundError(f"missing official input: {input_path}")
        input_text = input_path.read_text()
        old = run_solver(existing, input_text)
        new = run_solver(formatted, input_text)
        existing_results.append(old)
        formatted_results.append(new)
        print(
            f"seed={seed:04d} existing={old.score} formatted={new.score} "
            f"moves={old.moves}/{new.moves}"
        )

    old_sum = sum(result.score for result in existing_results)
    new_sum = sum(result.score for result in formatted_results)
    wins = sum(new.score > old.score for old, new in zip(
        existing_results, formatted_results
    ))
    ties = sum(new.score == old.score for old, new in zip(
        existing_results, formatted_results
    ))
    losses = args.cases - wins - ties
    print(
        f"existing average={old_sum / args.cases:.2f} sum={old_sum}"
    )
    print(
        f"formatted average={new_sum / args.cases:.2f} sum={new_sum} "
        f"win/tie/loss={wins}/{ties}/{losses}"
    )


if __name__ == "__main__":
    main()
