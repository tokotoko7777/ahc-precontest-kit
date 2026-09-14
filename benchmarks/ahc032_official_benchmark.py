#!/usr/bin/env python3
"""Official AHC032 scores: old practice, old template, improved template."""
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
    parser.add_argument("--tool", type=Path, required=True, help="official AHC032 vis")
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.cases <= 0 or args.first_seed < 0:
        parser.error("positive cases and non-negative first-seed required")
    if args.output.exists(): parser.error("output exists; use a new CSV path")
    inputs, tool = args.inputs.resolve(), args.tool.resolve()
    for seed in range(args.first_seed, args.first_seed + args.cases):
        path = inputs / f"{seed:04d}.txt"
        if path.read_text().splitlines()[0].split() != ["9", "20", "81"]:
            parser.error(f"not an official AHC032 input: {path}")
    reference = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ahc032-official-score-") as temporary:
        work = Path(temporary)
        sources = {"practice_old": work / "practice_old.cpp", "template_old": work / "template_old.cpp", "formatted": ROOT / "examples/search/ahc032_action_beam.cpp"}
        for label, path in [("practice_old", "practice/ahc032/main.cpp"), ("template_old", "examples/search/ahc032_action_beam.cpp")]:
            content = subprocess.check_output(["git", "show", f"{reference}:{path}"], cwd=ROOT)
            if label == "template_old":
                # Freeze its header too, so later library changes cannot alter the reference.
                header = subprocess.check_output(["git", "show", f"{reference}:library/action-beam-search.hpp"], cwd=ROOT)
                content = content.replace(b'#include "../../library/action-beam-search.hpp"', header)
            sources[label].write_bytes(content)
        for label, source in sources.items():
            subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra", str(source), "-o", str(work / label)], check=True)
        totals = {label: 0 for label in sources}
        maximum_seconds = {label: 0.0 for label in sources}
        comparisons = {label: [0, 0, 0] for label in ("practice_old", "template_old")}
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=["seed", "version", "score", "seconds", "input_sha256", "output_sha256", "source_sha256", "reference_commit"])
            writer.writeheader()
            for seed in range(args.first_seed, args.first_seed + args.cases):
                path = inputs / f"{seed:04d}.txt"
                scores = {}
                order = list(sources) if seed % 2 == 0 else list(reversed(sources))
                for label in order:
                    score, seconds, digest = official_score("032", tool, work / label, path, work, label)
                    scores[label] = score
                    totals[label] += score
                    maximum_seconds[label] = max(maximum_seconds[label], seconds)
                    writer.writerow(dict(seed=seed, version=label, score=score, seconds=f"{seconds:.6f}", input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), output_sha256=digest, source_sha256=hashlib.sha256(sources[label].read_bytes()).hexdigest(), reference_commit=reference))
                destination.flush()
                for label, counts in comparisons.items():
                    delta = scores["formatted"] - scores[label]
                    counts[0 if delta > 0 else 1 if delta == 0 else 2] += 1
                print(f"seed={seed:04d} scores={scores}", flush=True)
        for label, total in totals.items():
            print(f"{label}: sum={total} average={total / args.cases:.2f} max_seconds={maximum_seconds[label]:.3f}")
        print(f"formatted W/T/L={comparisons}; CSV={args.output}")

if __name__ == "__main__": main()
