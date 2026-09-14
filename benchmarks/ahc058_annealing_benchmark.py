#!/usr/bin/env python3
"""Official AHC058 scores: fixed baseline versus prefix-replay annealing."""
from __future__ import annotations
import argparse
import csv
import hashlib
from pathlib import Path
import subprocess
import tempfile
from rollout_official_benchmark import ROOT, official_score

BASE = "f56782586166350002812bf499ec6ac7b9ce96b0"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True, help="official AHC058 vis")
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--solver", type=Path, default=ROOT / "examples/search/ahc058_prefix_sa.cpp")
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.cases <= 0 or args.first_seed < 0 or args.repeats <= 0:
        parser.error("positive cases/repeats and non-negative first-seed required")
    if args.output.exists(): parser.error("output exists; use a new CSV path")
    inputs, tool = args.inputs.resolve(), args.tool.resolve()
    for seed in range(args.first_seed, args.first_seed + args.cases):
        path = inputs / f"{seed:04d}.txt"
        if path.read_text().splitlines()[0].split() != ["10", "4", "500", "1"]:
            parser.error(f"not an official AHC058 input: {path}")
    reference = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ahc058-prefix-score-") as temporary:
        work = Path(temporary)
        old = work / "reference.cpp"
        old.write_bytes(subprocess.check_output(["git", "show", f"{reference}:practice/ahc058/main.cpp"], cwd=ROOT))
        sources = {"reference": old, "prefix_sa": args.solver.resolve()}
        for label, source in sources.items():
            subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra", str(source), "-o", str(work / label)], check=True)
        totals = {label: 0 for label in sources}
        win = tie = loss = 0
        maximum_seconds = {label: 0.0 for label in sources}
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=["seed", "repeat", "version", "score", "seconds", "input_sha256", "output_sha256", "source_sha256", "reference_commit"])
            writer.writeheader()
            for seed in range(args.first_seed, args.first_seed + args.cases):
                path = inputs / f"{seed:04d}.txt"
                for repeat in range(args.repeats):
                    scores = {}
                    # 実行順を交互にし、時間ベース探索の測定順依存を減らす。
                    order = list(sources) if (seed + repeat) % 2 == 0 else list(reversed(sources))
                    for label in order:
                        score, seconds, digest = official_score("058", tool, work / label, path, work, label)
                        scores[label] = score
                        totals[label] += score
                        maximum_seconds[label] = max(maximum_seconds[label], seconds)
                        writer.writerow(dict(seed=seed, repeat=repeat, version=label, score=score, seconds=f"{seconds:.6f}", input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), output_sha256=digest, source_sha256=hashlib.sha256(sources[label].read_bytes()).hexdigest(), reference_commit=reference))
                    destination.flush()
                    difference = scores["prefix_sa"] - scores["reference"]
                    win += difference > 0
                    tie += difference == 0
                    loss += difference < 0
                    print(f"seed={seed:04d} repeat={repeat} old={scores['reference']} new={scores['prefix_sa']} delta={difference}", flush=True)
        for label, total in totals.items():
            print(f"{label}: sum={total} average={total / (args.cases * args.repeats):.2f} max_seconds={maximum_seconds[label]:.3f}")
        print(f"W/T/L={win}/{tie}/{loss}; CSV={args.output}")

if __name__ == "__main__": main()
