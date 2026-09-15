#!/usr/bin/env python3
"""Generate AHC001 cases with official tools, validate output, and record scores."""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
from ahc001_gap_report import ROOT, REFERENCE, gap
from rollout_official_benchmark import run

BASE = "1e3e144ad9289ce0a0ccc5d52e6b2a6d13094f6a"

def validate(input_path, output_path):
    data = list(map(int, input_path.read_text().split()))
    n = data[0]
    if not 50 <= n <= 200 or len(data) != 1 + 3 * n: raise ValueError("not AHC001 input")
    values = list(map(int, output_path.read_text().split()))
    if len(values) != 4 * n: raise ValueError("expected exactly n rectangles")
    rectangles = [values[4*i:4*i+4] for i in range(n)]
    for i, (a,b,c,d) in enumerate(rectangles):
        x,y,_ = data[1+3*i:4+3*i]
        if not (0 <= a <= x < c <= 10000 and 0 <= b <= y < d <= 10000):
            raise ValueError(f"invalid rectangle {i}")
        for e,f,g,h in rectangles[:i]:
            if max(a,e) < min(c,g) and max(b,f) < min(d,h): raise ValueError("overlap")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", type=Path, required=True, help="official tools directory with target/release/gen and vis")
    parser.add_argument("--seeds", type=Path, required=True)
    parser.add_argument("--suite", choices=["development", "system"], default="development")
    parser.add_argument("--first-case", type=int, default=0)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--solver", type=Path, default=ROOT / "examples/search/ahc001_region_sa.cpp")
    parser.add_argument("--skip-reference", action="store_true")
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--threshold-table-size", type=int, default=0)
    parser.add_argument("--compare-threshold-table", action="store_true",
                        help="compile the same solver twice: requested table vs disabled table")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    bins = args.threshold_table_size
    if bins < 0 or bins > 2**20 or (bins and bins & (bins - 1)):
        parser.error("threshold table size must be 0 or a power of two up to 2^20")
    if args.compare_threshold_table and (args.skip_reference or bins == 0):
        parser.error("table comparison needs a nonzero table and cannot skip reference")
    seed_bytes = args.seeds.read_bytes()
    seed_md5 = hashlib.md5(seed_bytes).hexdigest()
    seeds = [int(s) for s in seed_bytes.split()]
    if args.suite == "system" and (len(seeds) != 1000 or seed_md5 != REFERENCE["system"]["seed_md5"]):
        parser.error("system suite requires the exact official 1000-seed manifest")
    if not 1 <= args.cases <= 1000 or args.first_case < 0 or args.first_case + args.cases > len(seeds):
        parser.error("case range is outside the seed manifest")
    if args.output.exists(): parser.error("output already exists; choose a new path")
    tools = args.tools.resolve() / "target/release"
    if not all((tools / name).is_file() for name in ("gen", "vis")): parser.error("build official gen and vis first")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    reference = ""
    if not args.skip_reference and not args.compare_threshold_table:
        reference = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    with tempfile.TemporaryDirectory(prefix="ahc001-score-") as directory:
        work = Path(directory)
        chosen = seeds[args.first_case:args.first_case + args.cases]
        # Generate inside a fresh directory: never overwrite tools/in or the user's inputs.
        subprocess.run([str(tools / "gen")], input="\n".join(map(str,chosen))+"\n", cwd=work, text=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=True)
        sources = {"region_sa": args.solver.resolve()}
        if args.compare_threshold_table:
            sources["reference"] = args.solver.resolve()
        elif not args.skip_reference:
            sources["reference"] = work / "reference.cpp"
            sources["reference"].write_bytes(subprocess.check_output(["git", "show", f"{reference}:practice/ahc001/main.cpp"], cwd=ROOT))
        for label, source in sources.items():
            table_size = bins if label == "region_sa" else 0
            subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                            f"-DAHC001_THRESHOLD_TABLE_SIZE={table_size}", str(source), "-o", str(work / label)], check=True)
        totals = {label:0 for label in sources}
        maximum = {label:0.0 for label in sources}
        wins = ties = losses = 0
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=["case_index","seed","suite","version","score","seconds","over_5s","input_sha256","output_sha256","source_sha256","seed_manifest_md5","reference_commit","threshold_table_size"])
            writer.writeheader()
            for offset, seed in enumerate(chosen):
                index = args.first_case + offset
                path = work / "in" / f"{offset:04d}.txt"
                scores = {}
                labels = list(sources) if index % 2 == 0 else list(reversed(sources))
                for label in labels:
                    output = work / f"{label}.out"
                    started = time.perf_counter()
                    with path.open() as data, output.open("w") as answer:
                        run([str(work / label)], stdin=data, stdout=answer, stderr=subprocess.PIPE, text=True, cwd=work)
                    seconds = time.perf_counter() - started
                    validate(path, output)
                    stdout, stderr = run([str(tools / "vis"), str(path), str(output)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=work)
                    if not stdout.strip().isdigit() or stderr.strip(): raise ValueError(f"official vis failed: {stdout} {stderr}")
                    score = int(stdout.strip())
                    scores[label] = score
                    totals[label] += score
                    maximum[label] = max(maximum[label], seconds)
                    writer.writerow(dict(case_index=index,seed=seed,suite=args.suite,version=label,score=score,seconds=f"{seconds:.6f}",over_5s=int(seconds>5),input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),source_sha256=hashlib.sha256(sources[label].read_bytes()).hexdigest(),seed_manifest_md5=seed_md5,reference_commit=reference,threshold_table_size=bins if label == "region_sa" else 0))
                    destination.flush()
                if "reference" in scores:
                    difference = scores["region_sa"] - scores["reference"]
                    wins += difference > 0; ties += difference == 0; losses += difference < 0
                print(f"case={index:04d} scores={scores}", flush=True)
        for label, total in totals.items():
            print(f"{label}: total={total} mean={total/args.cases:.2f} max_seconds={maximum[label]:.3f}")
        if not args.skip_reference: print(f"W/T/L={wins}/{ties}/{losses}")
        report = gap(totals["region_sa"], args.cases, "system")
        report["comparison"] = ("same published 1000 seeds, local hardware" if args.suite == "system" and args.cases == 1000
                                 else "REFERENCE ESTIMATE ONLY: incomplete or different inputs")
        if maximum["region_sa"] > 5:
            report["warning"] = "local runs exceeded 5 seconds; not an official time-valid total"
        print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == "__main__": main()
