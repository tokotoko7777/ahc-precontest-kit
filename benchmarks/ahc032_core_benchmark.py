#!/usr/bin/env python3
"""Compare beam-core changes by official AHC032 score and time-valid beam width.

Reference source is frozen including its header. Current candidates differ by
beam width; scores, validity, and per-input common-best ratios are primary.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
from pathlib import Path
import statistics
import subprocess
import tempfile
from rollout_official_benchmark import ROOT, official_score

BASE = "a208b60f9073e9a8599a21afc83fd483da5287b4"
MOD = 998244353


def replay_score(input_path, output_path):
    values = list(map(int, input_path.read_text().split()))
    if len(values) != 3 + 81 + 180 or values[:3] != [9, 20, 81]:
        raise ValueError("not an AHC032 input")
    board, stamps = values[3:84], values[84:]
    operations = list(map(int, output_path.read_text().split()))
    if not operations or not 0 <= operations[0] <= 81 or len(operations) != 1 + 3 * operations[0]:
        raise ValueError("invalid operation count")
    for offset in range(1, len(operations), 3):
        stamp, row, column = operations[offset:offset + 3]
        if not (0 <= stamp < 20 and 0 <= row < 7 and 0 <= column < 7):
            raise ValueError("invalid stamp or position")
        for y in range(3):
            for x in range(3):
                cell = (row + y) * 9 + column + x
                board[cell] = (board[cell] + stamps[stamp * 9 + y * 3 + x]) % MOD
    return sum(board)


def summarize(rows):
    # One common denominator per input/repeat, including every compared version.
    best = {}
    for row in rows:
        key = (row["seed"], row["repeat"])
        if not row["over_2s"]:
            best[key] = max(best.get(key, 0), row["score"])
    for version in dict.fromkeys(row["version"] for row in rows):
        selected = [row for row in rows if row["version"] == version]
        relative = sum(100 * row["score"] / best[row["seed"], row["repeat"]]
                       for row in selected if not row["over_2s"] and best.get((row["seed"], row["repeat"]), 0) > 0)
        print(f"{version}: mean_score={statistics.mean(row['score'] for row in selected):.2f} "
              f"common_best={relative:.6f}/{100 * len(selected)} "
              f"average_best_ratio={relative / len(selected):.6f}% "
              f"median_seconds={statistics.median(row['seconds'] for row in selected):.6f} "
              f"max_seconds={max(row['seconds'] for row in selected):.6f} "
              f"over_2s={sum(row['over_2s'] for row in selected)}")
    print("Ratios use the time-valid common best in this comparison, not official standings; "
          "time-invalid rows receive zero relative points. "
          "Do not select a time-invalid width by score alone.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True, help="official AHC032 vis")
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--reference-width", type=int, default=6000)
    parser.add_argument("--extra-reference-widths", type=int, nargs="*", default=[],
                        help="also tune the frozen core; do not credit spare baseline time as a core speedup")
    parser.add_argument("--widths", type=int, nargs="+", default=[6000])
    parser.add_argument("--label", default="current")
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    reference_widths = [args.reference_width, *args.extra_reference_widths]
    if min([*reference_widths, args.cases, args.repeats, *args.widths]) <= 0 or args.first_seed < 0:
        parser.error("positive widths/cases/repeats and nonnegative first-seed required")
    if len(args.widths) != len(set(args.widths)) or len(reference_widths) != len(set(reference_widths)):
        parser.error("duplicate widths")
    if not args.label.replace("_", "").replace("-", "").isalnum() or args.label == "reference":
        parser.error("label must be a filename-safe name other than reference")
    if args.output.exists(): parser.error("output exists; choose a new CSV path")
    inputs, tool = args.inputs.resolve(), args.tool.resolve()
    if not tool.is_file(): parser.error("official vis not found")
    for seed in range(args.first_seed, args.first_seed + args.cases):
        if not (inputs / f"{seed:04d}.txt").is_file(): parser.error(f"missing seed {seed}")
    reference = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    old = subprocess.check_output(["git", "show", f"{reference}:practice/ahc032/main.cpp"], cwd=ROOT, text=True)
    current = (ROOT / "examples/search/ahc032_action_beam.cpp").read_text()
    marker = '#include "../../library/action-beam-search.hpp"'
    if current.count(marker) != 1: raise ValueError("expected exactly one beam header")
    current = current.replace(marker, (ROOT / "library/action-beam-search.hpp").read_text())
    versions = {f"reference_w{width}": (old, width) for width in reference_widths}
    versions.update({f"{args.label}_w{width}": (current, width) for width in args.widths})
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ahc032-core-") as directory:
        work = Path(directory)
        for version, (source, width) in versions.items():
            path = work / f"{version}.cpp"
            path.write_text(source)
            subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                            f"-DAHC032_BEAM_WIDTH={width}", str(path), "-o", str(work / version)], check=True)
        rows = []
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=["seed", "repeat", "version", "width", "score", "seconds", "over_2s", "input_sha256", "output_sha256", "source_sha256", "reference_commit", "compiler", "flags"])
            writer.writeheader()
            for seed in range(args.first_seed, args.first_seed + args.cases):
                path = inputs / f"{seed:04d}.txt"
                for repeat in range(args.repeats):
                    order = list(versions)
                    shift = (seed + repeat) % len(order)
                    order = order[shift:] + order[:shift]
                    scores = {}
                    for version in order:
                        source, width = versions[version]
                        score, seconds, digest = official_score("032", tool, work / version, path, work, version)
                        if replay_score(path, work / f"{path.stem}-{version}.out") != score:
                            raise ValueError("independent replay disagrees with official score")
                        row = dict(seed=seed, repeat=repeat, version=version, width=width,
                                   score=score, seconds=seconds, over_2s=int(seconds > 2),
                                   input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                   output_sha256=digest, source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                                   reference_commit=reference, compiler=compiler,
                                   flags=f"-std=c++17 -O2 -DNDEBUG -Wall -Wextra -DAHC032_BEAM_WIDTH={width}")
                        rows.append(row)
                        writer.writerow(row)
                        destination.flush()
                        scores[version] = score
                    print(f"seed={seed:04d} repeat={repeat} scores={scores}", flush=True)
        summarize(rows)


if __name__ == "__main__": main()
