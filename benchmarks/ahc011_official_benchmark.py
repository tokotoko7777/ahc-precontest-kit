#!/usr/bin/env python3
"""Compare AHC011 solvers and independently replay every slide."""

from __future__ import annotations

import argparse
import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "ahc011-official"


@dataclass
class Result:
    score: int
    largest_tree: int
    moves: int


def compile_solver(source: Path, output: Path, strict: bool) -> None:
    command = [
        "g++", "-std=c++17", "-O3", "-DNDEBUG",
        "-Wall", "-Wextra", "-pedantic",
    ]
    if strict:
        command += ["-Wshadow", "-Wconversion", "-Werror", "-I", str(ROOT)]
    subprocess.run(command + [str(source), "-o", str(output)], check=True)


def rounded_fraction(numerator: int, denominator: int) -> int:
    return (2 * numerator + denominator) // (2 * denominator)


def score_output(input_text: str, output_text: str) -> Result:
    lines = input_text.splitlines()
    n, turn_limit = map(int, lines[0].split())
    board = [int(character, 16) for line in lines[1:1 + n]
             for character in line.strip()]
    commands = output_text.strip()
    if len(commands) > turn_limit:
        raise ValueError("output exceeds the turn limit")
    if any(command not in "UDLR" for command in commands):
        raise ValueError("output contains a character other than U/D/L/R")

    empty = board.index(0)
    direction = {
        "U": (-1, 0),
        "D": (1, 0),
        "L": (0, -1),
        "R": (0, 1),
    }
    for turn, command in enumerate(commands, 1):
        row, column = divmod(empty, n)
        delta_row, delta_column = direction[command]
        next_row = row + delta_row
        next_column = column + delta_column
        if not (0 <= next_row < n and 0 <= next_column < n):
            raise ValueError(f"turn {turn}: moved outside the board")
        next_empty = next_row * n + next_column
        board[empty], board[next_empty] = board[next_empty], board[empty]
        empty = next_empty

    parent = [-1] * (n * n)
    cyclic = [False] * (n * n)

    def root(vertex: int) -> int:
        while parent[vertex] >= 0:
            if parent[parent[vertex]] >= 0:
                parent[vertex] = parent[parent[vertex]]
            vertex = parent[vertex]
        return vertex

    def add_edge(first: int, second: int) -> None:
        first = root(first)
        second = root(second)
        if first == second:
            cyclic[first] = True
            return
        if parent[first] > parent[second]:
            first, second = second, first
        has_cycle = cyclic[first] or cyclic[second]
        parent[first] += parent[second]
        parent[second] = first
        cyclic[first] = has_cycle

    for row in range(n):
        for column in range(n):
            cell = row * n + column
            tile = board[cell]
            if column + 1 < n and tile & 4 and board[cell + 1] & 1:
                add_edge(cell, cell + 1)
            if row + 1 < n and tile & 8 and board[cell + n] & 2:
                add_edge(cell, cell + n)

    largest_tree = 0
    for cell in range(n * n):
        if board[cell] != 0 and root(cell) == cell and not cyclic[cell]:
            largest_tree = max(largest_tree, -parent[cell])

    if largest_tree < n * n - 1:
        score = rounded_fraction(500000 * largest_tree, n * n - 1)
    else:
        score = rounded_fraction(
            1000000 * turn_limit - 500000 * len(commands), turn_limit
        )
    return Result(score, largest_tree, len(commands))


def run_solver(executable: Path, input_text: str) -> Result:
    completed = subprocess.run(
        [str(executable)], input=input_text, text=True,
        capture_output=True, timeout=5.0, check=True
    )
    return score_output(input_text, completed.stdout)


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
    compile_solver(ROOT / "practice/ahc011/main.cpp", existing, strict=False)
    compile_solver(
        ROOT / "examples/search/ahc011_tree_beam.cpp", formatted, strict=True
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
            f"tree={old.largest_tree}/{new.largest_tree} "
            f"moves={old.moves}/{new.moves}"
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


if __name__ == "__main__":
    main()
