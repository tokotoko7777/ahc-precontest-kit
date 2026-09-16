#!/usr/bin/env python3
"""AHC015 official score under a shared 2 s limit; no future input is sent early."""
from __future__ import annotations
import argparse
import csv
import hashlib
import os
from pathlib import Path
import re
import selectors
import signal
import statistics
import subprocess
import tempfile
import time
from rollout_official_benchmark import ROOT, run

BASE = "8b79c5c30b5447a511bebeb2d55164bda8362887"


def replay_score(values, actions):
    if len(values) != 200 or any(not 1 <= x <= 3 for x in values[:100]):
        raise ValueError("not an AHC015 input")
    if len(actions) != 100 or any(action not in ("F", "B", "L", "R") for action in actions):
        raise ValueError("expected 100 directions")
    board = [[0] * 10 for _ in range(10)]
    for turn, action in enumerate(actions):
        empty = [(r, c) for r in range(10) for c in range(10) if board[r][c] == 0]
        rank = values[100 + turn]
        if not 1 <= rank <= len(empty):
            raise ValueError("invalid placement rank")
        r, c = empty[rank - 1]
        board[r][c] = values[turn]
        for line in range(10):
            cells = ([(i, line) for i in range(10)] if action in ("F", "B")
                     else [(line, i) for i in range(10)])
            if action in ("B", "R"): cells.reverse()
            candy = [board[r][c] for r, c in cells if board[r][c]]
            for i, (r, c) in enumerate(cells): board[r][c] = candy[i] if i < len(candy) else 0
    visited = set()
    numerator = 0
    for r in range(10):
        for c in range(10):
            if (r, c) in visited: continue
            visited.add((r, c))
            pending = [(r, c)]
            count = 0
            while pending:
                a, b = pending.pop()
                count += 1
                for u, v in ((a - 1, b), (a + 1, b), (a, b - 1), (a, b + 1)):
                    if 0 <= u < 10 and 0 <= v < 10 and (u, v) not in visited and board[u][v] == board[r][c]:
                        visited.add((u, v))
                        pending.append((u, v))
            numerator += count * count
    denominator = sum(values[:100].count(color) ** 2 for color in (1, 2, 3))
    return (2 * 1000000 * numerator + denominator) // (2 * denominator)


def interactive(executable, values):
    started = time.perf_counter()
    deadline = started + 30
    with subprocess.Popen([str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, start_new_session=True) as process:
        try:
            process.stdin.write((" ".join(map(str, values[:100])) + "\n").encode())
            actions, pending = [], b""
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                for rank in values[100:]:
                    if pending: raise ValueError("solver printed a future turn before receiving it")
                    process.stdin.write(f"{rank}\n".encode())
                    process.stdin.flush()
                    while b"\n" not in pending:
                        remaining = deadline - time.perf_counter()
                        if remaining <= 0 or not selector.select(remaining): raise TimeoutError("solver did not flush a turn")
                        chunk = os.read(process.stdout.fileno(), 4096)
                        if not chunk: raise ValueError("solver exited before 100 turns")
                        pending += chunk
                    line, pending = pending.split(b"\n", 1)
                    action = line.decode().strip()
                    if action not in ("F", "B", "L", "R"): raise ValueError("invalid direction")
                    actions.append(action)
            process.stdin.close()
            process.wait(timeout=max(0.01, deadline - time.perf_counter()))
            stderr = process.stderr.read().decode()
            extra = pending + process.stdout.read()
            if process.returncode or extra.strip(): raise ValueError(f"solver failed or printed extra output: {stderr}")
            return actions, time.perf_counter() - started, stderr
        except BaseException:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            raise


def summarize(rows):
    best = {}
    for row in rows:
        key = row["seed"], row["repeat"]
        if not row["over_2s"]: best[key] = max(best.get(key, 0), row["score"])
    for version in dict.fromkeys(row["version"] for row in rows):
        selected = [row for row in rows if row["version"] == version]
        relative = sum(100 * row["score"] / best[row["seed"], row["repeat"]] for row in selected
                       if not row["over_2s"] and best.get((row["seed"], row["repeat"]), 0))
        print(f"{version}: mean_score={statistics.mean(r['score'] for r in selected):.2f} "
              f"common_best={relative:.6f}/{100 * len(selected)} average_best_ratio={relative / len(selected):.6f}% "
              f"median_seconds={statistics.median(r['seconds'] for r in selected):.6f} "
              f"max_seconds={max(r['seconds'] for r in selected):.6f} over_2s={sum(r['over_2s'] for r in selected)} "
              f"mean_transitions={statistics.mean(r['transitions'] for r in selected):.1f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True, help="official vis")
    parser.add_argument("--samples", type=int, nargs="+", default=[128, 256])
    parser.add_argument("--reference-samples", type=int, nargs="+", default=[128, 256])
    parser.add_argument("--include-unmerged", action="store_true")
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--cases", type=int, default=5)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if min(args.samples + args.reference_samples + [args.cases, args.repeats]) <= 0 or args.first_seed < 0:
        parser.error("positive counts and nonnegative first seed required")
    if args.output.exists(): parser.error("output exists")
    example = (ROOT / "examples/search/ahc015_common_rollout.cpp").read_text()
    source = example
    for header in ("common-scenario-average.hpp", "coalesced-rollout.hpp"):
        source = source.replace(f'#include "../../library/{header}"', (ROOT / "library" / header).read_text())
    # Frozen original Problem and original core; only the same input/output wrapper is added.
    frozen = subprocess.check_output(["git", "show", f"{BASE}:benchmarks/ahc015_monte_carlo_score_benchmark.cpp"], cwd=ROOT, text=True)
    frozen = frozen[:frozen.index("Board run_rule_policy")]
    old_header = subprocess.check_output(["git", "show", f"{BASE}:library/common-scenario-average.hpp"], cwd=ROOT, text=True)
    frozen = frozen.replace('#include "library/common-scenario-average.hpp"', old_header)
    # The unused helper can be declared in the classic wrapper; no new code is used by its rollout.
    frozen += (ROOT / "library/coalesced-rollout.hpp").read_text()
    frozen += example[example.index("#ifndef AHC015_SAMPLES"):]
    versions = {f"reference_s{n}": (frozen, n, 1, 0) for n in args.reference_samples}
    versions.update({f"merged_s{n}": (source, n, 0, 1) for n in args.samples})
    if args.include_unmerged:
        versions.update({f"unmerged_s{n}": (source, n, 0, 0) for n in args.samples})
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ahc015-score-") as directory:
        work = Path(directory)
        metadata = {}
        for label, (code, samples, classic, merge) in versions.items():
            cpp = work / f"{label}.cpp"
            cpp.write_text(code)
            flags = ["-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                     f"-DAHC015_SAMPLES={samples}", f"-DAHC015_CLASSIC={classic}", f"-DAHC015_MERGE={merge}",
                     "-DAHC015_TURN_SEED=1", "-DAHC015_ENGINE_SEED=15015"]
            subprocess.run(["g++", *flags, str(cpp), "-o", str(work / label)], check=True)
            metadata[label] = dict(samples=samples, classic=classic, merge=merge, source_sha256=hashlib.sha256(code.encode()).hexdigest(),
                                   reference_commit=BASE, compiler=compiler, flags=" ".join(flags))
        rows = []
        with args.output.open("x", newline="") as destination:
            writer = csv.DictWriter(destination, lineterminator="\n", fieldnames=["seed", "repeat", "version", "samples", "classic", "merge", "score", "seconds", "over_2s", "transitions", "input_sha256", "output_sha256", "source_sha256", "reference_commit", "compiler", "flags"])
            writer.writeheader()
            for seed in range(args.first_seed, args.first_seed + args.cases):
                path = args.inputs.resolve() / f"{seed:04d}.txt"
                values = list(map(int, path.read_text().split()))
                if len(values) != 200: raise ValueError("not AHC015 input")
                for repeat in range(args.repeats):
                    labels = list(versions)
                    shift = (seed + repeat) % len(labels)
                    scores = {}
                    digests = {}
                    for label in labels[shift:] + labels[:shift]:
                        actions, seconds, stderr = interactive(work / label, values)
                        output = work / f"{label}.out"
                        output.write_text("\n".join(actions) + "\n")
                        stdout, err = run([str(args.tool.resolve()), str(path), str(output)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=work)
                        found = re.findall(r"^Score\s*=\s*(\d+)\s*$", stdout + "\n" + err, re.MULTILINE)
                        if len(found) != 1 or int(found[0]) != replay_score(values, actions):
                            raise ValueError(f"official score differs from independent replay: {stdout} {err}")
                        transitions = re.search(r"transitions=(\d+)", stderr)
                        if transitions is None: raise ValueError("missing transition counter")
                        row = dict(seed=seed, repeat=repeat, version=label, score=int(found[0]), seconds=seconds, over_2s=int(seconds > 2),
                                   transitions=int(transitions[1]), input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                   output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(), **metadata[label])
                        writer.writerow(row)
                        destination.flush()
                        rows.append(row)
                        scores[label], digests[label] = row["score"], row["output_sha256"]
                    for n in set(args.samples) & set(args.reference_samples):
                        if digests[f"reference_s{n}"] != digests[f"merged_s{n}"]: raise ValueError("same samples changed output")
                    if args.include_unmerged:
                        for n in args.samples:
                            if digests[f"unmerged_s{n}"] != digests[f"merged_s{n}"]: raise ValueError("merge changed output")
                    print(f"seed={seed:04d} repeat={repeat} scores={scores}", flush=True)
        summarize(rows)


if __name__ == "__main__": main()
