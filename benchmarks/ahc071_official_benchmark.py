#!/usr/bin/env python3
"""Run the AHC071 example on official inputs and score with the official vis."""

from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


SCORE_PATTERN = re.compile(r"Score\s*=\s*(\d+)")


def compile_cpp(source: Path, output: Path, time_limit: float | None) -> None:
    command = [
        "g++",
        "-std=c++17",
        "-O3",
        "-DNDEBUG",
        "-march=native",
    ]
    if time_limit is not None:
        command.append(f"-DAHC071_TIME_LIMIT={time_limit}")
    command.extend([str(source), "-o", str(output)])
    subprocess.run(command, check=True)


def find_visualizer(tools: Path) -> Path:
    visualizer = tools / "target" / "release" / "vis"
    if not visualizer.exists():
        subprocess.run(
            ["cargo", "build", "--release", "--bin", "vis"],
            cwd=tools,
            check=True,
        )
    return visualizer


def run_one(
    binary: Path,
    binary_arguments: list[str],
    input_path: Path,
    output_path: Path,
    timeout: float,
) -> None:
    with input_path.open("rb") as input_file, output_path.open("wb") as output_file:
        subprocess.run(
            [str(binary), *binary_arguments],
            stdin=input_file,
            stdout=output_file,
            stderr=subprocess.DEVNULL,
            timeout=timeout,
            check=True,
        )


def official_score(visualizer: Path, input_path: Path, output_path: Path) -> int:
    completed = subprocess.run(
        [str(visualizer), str(input_path), str(output_path), "--no-vis"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    match = SCORE_PATTERN.search(completed.stdout)
    if match is None:
        raise RuntimeError(f"could not parse score: {completed.stdout!r}")
    return int(match.group(1))


def ported_score(input_path: Path, output_path: Path) -> int:
    """Exact Python port of the validation and score rules in tools/src/lib.rs."""
    data = list(map(int, input_path.read_text().split()))
    width, height, hole_count = data[:3]
    costs = data[3:8]
    holes = list(zip(data[8::2], data[9::2]))
    if len(holes) != hole_count:
        raise RuntimeError("invalid official input")

    output = list(map(int, output_path.read_text().split()))
    if not output or not 0 <= output[0] <= width * height:
        raise RuntimeError("invalid brick count")
    if len(output) != 1 + 3 * output[0]:
        raise RuntimeError("invalid output length")

    occupied = [[False] * width for _ in range(height)]
    bricks = list(zip(output[1::3], output[2::3], output[3::3]))
    total_cost = 0
    for x, y, brick_width in bricks:
        if brick_width not in (1, 3, 5, 7, 9):
            raise RuntimeError("invalid brick width")
        if not (0 <= x and x + brick_width <= width and 0 <= y < height):
            raise RuntimeError("brick is outside the wall")
        for column in range(x, x + brick_width):
            if occupied[y][column]:
                raise RuntimeError("bricks overlap")
            occupied[y][column] = True
        total_cost += costs[brick_width // 2]

    for x, y, brick_width in bricks:
        if y > 0 and not occupied[y - 1][x + brick_width // 2]:
            raise RuntimeError("brick is not supported")
    for x, y in holes:
        if not occupied[y][x]:
            raise RuntimeError("hole is not covered")
    return max(0, width * height * costs[0] - total_cost + 1)


def main() -> None:
    repository = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--tools",
        type=Path,
        required=True,
        help="AHC071 official tools directory containing in/ and Cargo.toml",
    )
    parser.add_argument(
        "--solver",
        type=Path,
        default=repository / "examples/search/ahc071_action_beam.cpp",
    )
    parser.add_argument(
        "--reference",
        type=Path,
        help="optional C++ solver to compare, for example AHC071/main3.cpp",
    )
    parser.add_argument(
        "--visualizer",
        type=Path,
        help="optional already-built official vis binary",
    )
    parser.add_argument(
        "--ported-score",
        action="store_true",
        help="use the bundled exact Python port of tools/src/lib.rs",
    )
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--time", type=float, default=1.8)
    args = parser.parse_args()

    if args.cases <= 0 or args.cases > 100:
        parser.error("--cases must be in 1..100")
    if args.time <= 0:
        parser.error("--time must be positive")
    args.tools = args.tools.resolve()
    visualizer = None
    if not args.ported_score:
        visualizer = (
            args.visualizer.resolve()
            if args.visualizer is not None
            else find_visualizer(args.tools)
        )

    with tempfile.TemporaryDirectory(prefix="ahc071-benchmark-") as directory:
        temporary = Path(directory)
        solver_binary = temporary / "solver"
        compile_cpp(args.solver.resolve(), solver_binary, args.time)

        binaries = [("kit", solver_binary, [])]
        if args.reference is not None:
            reference_binary = temporary / "reference"
            # main3.cpp has its own 1.8 second default and command-line parser.
            compile_cpp(args.reference.resolve(), reference_binary, None)
            binaries.append(
                ("reference", reference_binary, ["--time", str(args.time)])
            )

        scores: dict[str, list[int]] = {
            name: [] for name, _, _ in binaries
        }
        for case in range(args.cases):
            input_path = args.tools / "in" / f"{case:04d}.txt"
            row = [f"{case:04d}"]
            for name, binary, binary_arguments in binaries:
                output_path = temporary / f"{name}-{case:04d}.txt"
                run_one(
                    binary,
                    binary_arguments,
                    input_path,
                    output_path,
                    args.time + 5.0,
                )
                score = (
                    ported_score(input_path, output_path)
                    if visualizer is None
                    else official_score(visualizer, input_path, output_path)
                )
                scores[name].append(score)
                row.append(f"{name}={score}")
            print(" ".join(row), flush=True)

        print("summary")
        for name, values in scores.items():
            print(
                f"{name}: average={sum(values) / len(values):.2f} "
                f"sum={sum(values)}"
            )
        if len(binaries) == 2:
            left = scores[binaries[0][0]]
            right = scores[binaries[1][0]]
            wins = sum(a > b for a, b in zip(left, right))
            ties = sum(a == b for a, b in zip(left, right))
            losses = len(left) - wins - ties
            print(f"kit vs reference: {wins}/{ties}/{losses} (win/tie/loss)")


if __name__ == "__main__":
    main()
