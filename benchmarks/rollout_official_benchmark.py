#!/usr/bin/env python3
"""Compare AHC058/061 revisions using the official vis/tester score.

The primary metric is score, never rollout throughput. The two solvers run
sequentially per case. AHC061 uses the interactive tester, not stdin replay.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = "48e905a1b61eb70051d9fb7af18d34e13390cb69"
EXAMPLES = {"058": "ahc058_deterministic_rollout.cpp",
            "061": "ahc061_common_rollout.cpp"}


def run(command, **kwargs):
    # Kill the entire group on timeout, including an interactive tester's child.
    with subprocess.Popen(command, start_new_session=True, **kwargs) as process:
        try:
            stdout, stderr = process.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
            raise
        if process.returncode:
            raise RuntimeError(f"{command}: exit {process.returncode}: {stderr}")
        return stdout or "", stderr or ""


def official_score(task, tool, executable, input_path, work, label):
    output_path = work / f"{input_path.stem}-{label}.out"
    started = time.perf_counter()
    with input_path.open() as source, output_path.open("w") as output:
        command = ([str(tool), str(executable)] if task == "061"
                   else [str(executable)])
        _, stderr = run(command, stdin=source, stdout=output,
                        stderr=subprocess.PIPE, text=True, cwd=work)
    elapsed = time.perf_counter() - started
    if task == "058":
        stdout, stderr = run([str(tool), str(input_path), str(output_path)],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, cwd=work)
        report = stdout + "\n" + stderr
    else:
        report = stderr
    (work / f"{input_path.stem}-{label}.log").write_text(report)
    scores = re.findall(r"^Score\s*=\s*(\d+)\s*$", report, re.MULTILINE)
    if re.search(r"^Error:", report, re.MULTILINE) or len(scores) != 1 or int(scores[0]) <= 0:
        raise RuntimeError(f"official tool did not report one positive score: {report}")
    return int(scores[0]), elapsed, hashlib.sha256(output_path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task", required=True, choices=EXAMPLES)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True, help="058: vis; 061: tester")
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--output", type=Path, required=True, help="new CSV (never overwritten)")
    args = parser.parse_args()
    if not 1 <= args.cases <= 10000:
        parser.error("--cases must be from 1 to 10000")
    if args.output.exists():
        parser.error("--output already exists; choose a new path")
    inputs, tool = args.inputs.resolve(), args.tool.resolve()
    if not tool.is_file():
        parser.error("official tool not found")
    for seed in range(args.cases):
        if not (inputs / f"{seed:04d}.txt").is_file():
            parser.error(f"missing seed {seed}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    reference_sha = subprocess.check_output(
        ["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"],
        cwd=ROOT, text=True).strip()
    # Preserve the old solver without adding another permanent submission file.
    with tempfile.TemporaryDirectory(prefix=f"ahc{args.task}-rollout-") as directory:
        work = Path(directory)
        old_source = work / "reference.cpp"
        old_source.write_bytes(subprocess.check_output(
            ["git", "show", f"{reference_sha}:practice/ahc{args.task}/main.cpp"], cwd=ROOT))
        sources = {"reference": old_source,
                   "formatted": ROOT / "examples/search" / EXAMPLES[args.task],
                   "standalone": ROOT / f"practice/ahc{args.task}/main.cpp"}
        executables = {}
        for label, source in sources.items():
            executable = work / label
            subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG",
                            "-Wall", "-Wextra", str(source), "-o", str(executable)], check=True)
            executables[label] = executable
        totals = {label: 0 for label in sources}
        wins = ties = losses = 0
        with args.output.open("x", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=[
                "task", "seed", "version", "score", "seconds", "input_sha256",
                "output_sha256", "source_sha256", "reference_commit"])
            writer.writeheader()
            for seed in range(args.cases):
                input_path = inputs / f"{seed:04d}.txt"
                # Catch accidentally using another contest's tools/inputs early.
                first = list(map(int, input_path.read_text().splitlines()[0].split()))
                if args.task == "058" and first != [10, 4, 500, 1]:
                    raise ValueError(f"not an AHC058 input: {first}")
                if args.task == "061" and not (len(first) == 4 and first[0] == 10 and first[2] == 100):
                    raise ValueError(f"not an AHC061 input: {first}")
                results = {}
                for label, executable in executables.items():
                    score, seconds, digest = official_score(
                        args.task, tool, executable, input_path, work, label)
                    results[label] = (score, digest)
                    totals[label] += score
                    writer.writerow(dict(task=args.task, seed=seed, version=label,
                        score=score, seconds=f"{seconds:.6f}",
                        input_sha256=hashlib.sha256(input_path.read_bytes()).hexdigest(),
                        output_sha256=digest,
                        source_sha256=hashlib.sha256(sources[label].read_bytes()).hexdigest(),
                        reference_commit=reference_sha))
                output.flush()
                if results["formatted"] != results["standalone"]:
                    raise RuntimeError(f"seed {seed}: example and standalone differ")
                old, new = results["reference"][0], results["formatted"][0]
                wins += new > old
                ties += new == old
                losses += new < old
                print(f"seed={seed:04d} reference={old} formatted={new} "
                      f"output_equal={results['reference'][1] == results['formatted'][1]}", flush=True)
        for label, total in totals.items():
            print(f"{label}: sum={total} average={total / args.cases:.2f}")
        print(f"formatted W/T/L={wins}/{ties}/{losses}; CSV={args.output}")


if __name__ == "__main__":
    main()
