#!/usr/bin/env python3
"""Compare only ActionBeamSearch core changes on the same AHC071 Problem.

Timed mode (default) measures actual solution scores. Fixed mode is an output
equivalence / throughput diagnostic, not the adoption criterion.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
from pathlib import Path
import re
import statistics
import subprocess
import tempfile
import time
from ahc071_official_benchmark import official_score, ported_score

ROOT = Path(__file__).resolve().parents[1]
BASE = "8b79c5c30b5447a511bebeb2d55164bda8362887"
EXAMPLE = "examples/search/ahc071_action_beam.cpp"
HEADERS = ["action-beam-search.hpp", "simulated-annealing.hpp"]
# Fixed six full-wall beams, unchanged candidate generation/evaluation/policy.
# LNS is deliberately absent here: no time-dependent annealing schedule/noise.
FIXED_MAIN = r'''
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  Solver solver;
  solver.read_input();
  solver.started = chrono::steady_clock::now();
  solver.build_potential();
  auto best = solver.greedy_solution();
  solver.polish(best);
  int best_cost = Solver::total_cost(best);
  const float scales[] = {1.0f, 1.5f, 0.7f, 2.0f, 1.2f, 0.85f};
  int completed = 0;
  for (int run = 0; run < 6; ++run) {
    auto candidate = solver.search_rows(0, solver.H - 1,
        solver.beam_width, solver.row_candidate_count, best_cost,
        scales[run], run < 3 ? 0.0f : 1.0f, 0, solver.full,
        solver.potential, nullptr, 1000.0);
    if (!candidate.has_value()) continue;
    ++completed;
    solver.polish(*candidate);
    int cost = Solver::total_cost(*candidate);
    if (cost < best_cost) { best_cost = cost; best = std::move(*candidate); }
  }
  solver.print_answer(best);
  cerr << "cost: " << best_cost << " beams: " << completed
       << " initial: " << best_cost << " rebuilds: 0 elapsed: "
       << solver.elapsed() * 1000 << "ms\n";
}
'''


def source_at(ref):
    def read(path):
        if ref is None:
            return (ROOT / path).read_text()
        return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)
    source = read(EXAMPLE)
    for header in HEADERS:
        marker = f'#include "../../library/{header}"'
        if source.count(marker) != 1:
            raise ValueError(f"expected one {header} include")
        source = source.replace(marker, read(f"library/{header}"))
    return source


def summarize(rows):
    best = {}
    for row in rows:
        key = row["seed"]
        if not row["over_2s"]:
            best[key] = max(best.get(key, 0), row["score"])
    for version in dict.fromkeys(row["version"] for row in rows):
        selected = [row for row in rows if row["version"] == version]
        relative = sum(100 * row["score"] / best[row["seed"]]
                       for row in selected if not row["over_2s"] and best.get(row["seed"], 0))
        print(f"{version}: mean_score={statistics.mean(r['score'] for r in selected):.6f} "
              f"common_best={relative:.6f}/{100 * len(selected)} "
              f"average_best_ratio={relative / len(selected):.6f}% "
              f"median_seconds={statistics.median(r['seconds'] for r in selected):.6f} "
              f"max_seconds={max(r['seconds'] for r in selected):.6f} "
              f"mean_beams={statistics.mean(r['beams'] for r in selected):.3f} "
              f"mean_rebuilds={statistics.mean(r['rebuilds'] for r in selected):.3f} "
              f"over_2s={sum(r['over_2s'] for r in selected)}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True, help="official AHC071 vis")
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--widths", nargs="+", type=int, default=[64])
    parser.add_argument("--reference-widths", nargs="+", type=int, default=[64])
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--mode", choices=["fixed", "timed"], default="timed")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists(): parser.error("output exists; choose a new CSV")
    if min(args.widths + args.reference_widths + [args.cases, args.repeats]) < 1 or args.first_seed < 0:
        parser.error("positive widths/cases/repeats and nonnegative first seed required")
    if len(set(args.widths)) != len(args.widths) or len(set(args.reference_widths)) != len(args.reference_widths):
        parser.error("duplicate widths")
    inputs, tool = args.inputs.resolve(), args.tool.resolve()
    if not tool.is_file(): parser.error("official vis not found")
    for seed in range(args.first_seed, args.first_seed + args.cases):
        if not (inputs / f"{seed:04d}.txt").is_file(): parser.error(f"missing seed {seed}")
    ref = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    # Problem and SA header must be identical. No solver-side optimization is
    # silently attributed to the generic beam core.
    for path in [EXAMPLE, "library/simulated-annealing.hpp"]:
        if (ROOT / path).read_text() != subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True):
            raise ValueError(f"Problem/SA changed versus reference: {path}")
    versions = {}
    for label, revision, widths in [("reference", ref, args.reference_widths), ("current", None, args.widths)]:
        source = source_at(revision)
        if args.mode == "fixed":
            source = source.replace("int main() {", "int unused_ahc071_main() {")
            source = source.replace("solver.print_answer(answer);", "solver.print_answer(answer);\n  return 0;")
            source += FIXED_MAIN
        for width in widths:
            versions[f"{label}_w{width}"] = (source, width)
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    tool_hash = hashlib.sha256(tool.read_bytes()).hexdigest()
    flags = ["-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-DAHC071_TIME_LIMIT=1.80"]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ahc071-key-score-") as directory:
        work = Path(directory)
        for version, (source, width) in versions.items():
            cpp = work / f"{version}.cpp"
            cpp.write_text(source)
            subprocess.run(["g++", *flags, f"-DAHC071_BEAM_WIDTH={width}", str(cpp), "-o", str(work / version)], check=True)
        rows = []
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=["seed", "repeat", "version", "mode", "width", "score", "seconds", "over_2s", "beams", "initial_cost", "rebuilds", "cost", "input_sha256", "output_sha256", "source_sha256", "tool_sha256", "reference_commit", "compiler", "flags"])
            writer.writeheader()
            for seed in range(args.first_seed, args.first_seed + args.cases):
                path = inputs / f"{seed:04d}.txt"
                for repeat in range(args.repeats):
                    order = list(versions)
                    shift = (seed + repeat) % len(order)
                    order = order[shift:] + order[:shift]
                    hashes = {}
                    for version in order:
                        source, width = versions[version]
                        output = work / f"{version}.out"
                        started = time.perf_counter()
                        with path.open("rb") as input_file, output.open("wb") as output_file:
                            result = subprocess.run([str(work / version)], stdin=input_file, stdout=output_file,
                                                    stderr=subprocess.PIPE, timeout=30, check=True)
                        seconds = time.perf_counter() - started
                        match = re.search(rb"cost: (\d+) beams: (\d+) initial: (\d+) rebuilds: (\d+)", result.stderr)
                        if match is None: raise ValueError(f"missing solver stats: {result.stderr!r}")
                        cost, beams, initial_cost, rebuilds = map(int, match.groups())
                        score = official_score(tool, path, output)
                        if score != ported_score(path, output): raise ValueError("official/independent score disagree")
                        digest = hashlib.sha256(output.read_bytes()).hexdigest()
                        if args.mode == "fixed" and width in hashes and hashes[width] != digest:
                            raise ValueError(f"fixed work output differs: seed={seed}, width={width}")
                        hashes[width] = digest
                        row = dict(seed=seed, repeat=repeat, version=version, mode=args.mode, width=width,
                                   score=score, seconds=seconds, over_2s=int(seconds > 2), beams=beams,
                                   initial_cost=initial_cost, rebuilds=rebuilds, cost=cost,
                                   input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), output_sha256=digest,
                                   source_sha256=hashlib.sha256(source.encode()).hexdigest(), tool_sha256=tool_hash,
                                   reference_commit=ref, compiler=compiler,
                                   flags=" ".join([*flags, f"-DAHC071_BEAM_WIDTH={width}"]))
                        rows.append(row)
                        writer.writerow(row)
                        destination.flush()
                    print(f"seed={seed:04d} repeat={repeat} " + " ".join(f"{r['version']}={r['score']}({r['rebuilds']} rebuilds)" for r in rows[-len(versions):]), flush=True)
        summarize(rows)
        print("Fixed mode is diagnostic only. Timed scores decide adoption; one common best per seed includes every time-valid variant/repeat, not official standings.")


if __name__ == "__main__": main()
