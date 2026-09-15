#!/usr/bin/env python3
"""Compare beam-core changes by official AHC021 score and time-valid beam width.

Both cores use the current Problem. Reference disables final-score selection;
--include-ranked-final also disables it in the new core to isolate that feature.
--include-original-benchmark freezes the previous Problem and core together.
Scores, validity, and per-input common-best ratios are primary.
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

BASE = "9c0252c95ab4767dba538d47cdfaf45be5796ee1"
def replay_score(input_path, output_path):
    board = list(map(int, input_path.read_text().split()))
    if sorted(board) != list(range(465)):
        raise ValueError("not an AHC021 input")
    out = list(map(int, output_path.read_text().split()))
    if not out or not 0 <= out[0] <= 10000 or len(out) != 1 + 4 * out[0]:
        raise ValueError("invalid operation count")
    for offset in range(1, len(out), 4):
        row, column, next_row, next_column = out[offset:offset + 4]
        if not (0 <= column <= row < 30 and 0 <= next_column <= next_row < 30):
            raise ValueError("invalid coordinate")
        if (next_row - row, next_column - column) not in ((0, 1), (0, -1), (1, 0), (1, 1), (-1, 0), (-1, -1)):
            raise ValueError("nonadjacent swap")
        a, b = row * (row + 1) // 2 + column, next_row * (next_row + 1) // 2 + next_column
        board[a], board[b] = board[b], board[a]
    errors = sum(board[row * (row + 1) // 2 + column] > board[(row + 1) * (row + 2) // 2 + column + delta]
                 for row in range(29) for column in range(row + 1) for delta in (0, 1))
    return 100000 - 5 * out[0] if errors == 0 else 50000 - 50 * errors


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
    parser.add_argument("--tool", type=Path, required=True, help="official AHC021 vis")
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--reference-width", type=int, default=40)
    parser.add_argument("--extra-reference-widths", type=int, nargs="*", default=[],
                        help="also tune the frozen core; do not credit spare baseline time as a core speedup")
    parser.add_argument("--widths", type=int, nargs="+", default=[40])
    parser.add_argument("--label", default="current")
    parser.add_argument("--include-legacy-practice", action="store_true")
    parser.add_argument("--include-ranked-final", action="store_true")
    parser.add_argument("--include-original-benchmark", action="store_true",
                        help="also freeze the former benchmark Problem, not just its tree core")
    parser.add_argument("--original-widths", type=int, nargs="+",
                        help="widths of the frozen benchmark; defaults to reference widths")
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    reference_widths = [args.reference_width, *args.extra_reference_widths]
    original_widths = args.original_widths or reference_widths
    if min([*reference_widths, *original_widths, args.cases, args.repeats, *args.widths]) <= 0 or args.first_seed < 0:
        parser.error("positive widths/cases/repeats and nonnegative first-seed required")
    if (len(args.widths) != len(set(args.widths)) or len(reference_widths) != len(set(reference_widths))
            or len(original_widths) != len(set(original_widths))):
        parser.error("duplicate widths")
    if (not args.label.replace("_", "").replace("-", "").isalnum()
            or args.label in ("reference", "original", "ranked", "legacy_practice")):
        parser.error("label must be filename-safe and must not use a reserved version name")
    if args.output.exists(): parser.error("output exists; choose a new CSV path")
    inputs, tool = args.inputs.resolve(), args.tool.resolve()
    if not tool.is_file(): parser.error("official vis not found")
    for seed in range(args.first_seed, args.first_seed + args.cases):
        if not (inputs / f"{seed:04d}.txt").is_file(): parser.error(f"missing seed {seed}")
    reference = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    example = (ROOT / "examples/search/ahc021_tree_beam.cpp").read_text()
    marker = '#include "../../library/tree-beam-search.hpp"'
    if example.count(marker) != 1: raise ValueError("expected exactly one beam header")
    old_header = subprocess.check_output(["git", "show", f"{reference}:library/tree-beam-search.hpp"], cwd=ROOT, text=True)
    ranked_prefix = "#define AHC021_FINAL_SCORE_SELECTION 0\n"
    old = ranked_prefix + example.replace(marker, old_header)
    current = example.replace(marker, (ROOT / "library/tree-beam-search.hpp").read_text())
    radix_marker = '#include "../../library/radix-heap.hpp"'
    radix = (ROOT / "library/radix-heap.hpp").read_text()
    old, current = old.replace(radix_marker, radix), current.replace(radix_marker, radix)
    versions = {f"reference_w{width}": (old, width) for width in reference_widths}
    versions.update({f"{args.label}_w{width}": (current, width) for width in args.widths})
    if args.include_ranked_final:
        versions.update({f"ranked_w{width}": (ranked_prefix + current, width) for width in args.widths})
    if args.include_original_benchmark:
        frozen = subprocess.check_output(["git", "show", f"{reference}:benchmarks/ahc021_tree_beam_score_benchmark.cpp"], cwd=ROOT, text=True)
        if frozen.count("std::array<int, CELL_COUNT> make_case") != 1:
            raise ValueError("reference benchmark no longer embeds its Problem; choose the historical reference commit")
        problem = frozen[:frozen.index("std::array<int, CELL_COUNT> make_case")]
        problem = problem.replace('#include "library/tree-beam-search.hpp"', old_header)
        # Only the stdin/stdout + replay wrapper is shared; the Problem and core are frozen.
        original = ranked_prefix + problem + example[example.index("struct PyramidResult"):]
        versions.update({f"original_w{width}": (original, width) for width in original_widths})
    if args.include_legacy_practice:
        legacy = subprocess.check_output(["git", "show", f"{reference}:practice/ahc021/main.cpp"], cwd=ROOT, text=True)
        versions["legacy_practice"] = (legacy, 0)  # its own historical width/time settings
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ahc021-core-") as directory:
        work = Path(directory)
        for version, (source, width) in versions.items():
            path = work / f"{version}.cpp"
            path.write_text(source)
            subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                            f"-DAHC021_BEAM_WIDTH={width}", str(path), "-o", str(work / version)], check=True)
        rows = []
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, lineterminator="\n", fieldnames=["seed", "repeat", "version", "width", "final_score_selection", "score", "seconds", "over_2s", "input_sha256", "output_sha256", "source_sha256", "reference_commit", "compiler", "flags"])
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
                        score, seconds, digest = official_score("021", tool, work / version, path, work, version)
                        if replay_score(path, work / f"{path.stem}-{version}.out") != score:
                            raise ValueError("independent replay disagrees with official score")
                        final_selection = (-1 if version == "legacy_practice" else
                                           int(not source.startswith(ranked_prefix)))
                        row = dict(seed=seed, repeat=repeat, version=version, width=width,
                                   final_score_selection=final_selection,
                                   score=score, seconds=seconds, over_2s=int(seconds > 2),
                                   input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                   output_sha256=digest, source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                                   reference_commit=reference, compiler=compiler,
                                   flags=f"-std=c++17 -O2 -DNDEBUG -Wall -Wextra -DAHC021_BEAM_WIDTH={width}")
                        rows.append(row)
                        writer.writerow(row)
                        destination.flush()
                        scores[version] = score
                    if args.include_ranked_final:
                        for width in args.widths:
                            if scores[f"{args.label}_w{width}"] < scores[f"ranked_w{width}"]:
                                raise ValueError("final score selection became worse than rank-zero selection")
                    print(f"seed={seed:04d} repeat={repeat} scores={scores}", flush=True)
        summarize(rows)


if __name__ == "__main__": main()
