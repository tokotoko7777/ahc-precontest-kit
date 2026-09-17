#!/usr/bin/env python3
"""LNS ablations on official AHC059/AHC002 inputs, with independent replay.

Build all versions before sequential, rotated-order timed runs. Keep raw outputs,
errors, hashes and timeouts. Primary metric: per-input time-valid common-best score.
This is not a comparison against official standings (different test inputs).
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics
import subprocess
import time
from ahc002_official_benchmark import score_output as score_002

ROOT = Path(__file__).resolve().parents[1]
BASE = "8b79c5c30b5447a511bebeb2d55164bda8362887"
FLAGS = ["-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def score_059(input_text, output_text):
    numbers = list(map(int, input_text.split()))
    if not numbers or numbers[0] != 20 or len(numbers) != 401:
        raise ValueError("not an official AHC059 input")
    n = numbers[0]
    board = numbers[1:]
    if sorted(board) != [i // 2 for i in range(n * n)]:
        raise ValueError("invalid card multiplicities")
    commands = output_text.split()
    if len(commands) > 2 * n ** 3 or any(c not in ("U", "D", "L", "R", "Z", "X") for c in commands):
        raise ValueError("invalid commands/count")
    row = col = moves = 0
    stack = []
    direction = dict(U=(-1, 0), D=(1, 0), L=(0, -1), R=(0, 1))
    for turn, command in enumerate(commands):
        at = row * n + col
        if command in direction:
            dr, dc = direction[command]
            row += dr
            col += dc
            moves += 1
            if not (0 <= row < n and 0 <= col < n):
                raise ValueError(f"turn {turn}: outside board")
        elif command == "Z":
            if board[at] < 0:
                raise ValueError(f"turn {turn}: empty pickup")
            card, board[at] = board[at], -1
            if stack and stack[-1] == card:
                stack.pop()
            else:
                stack.append(card)
        else:
            if board[at] >= 0 or not stack:
                raise ValueError(f"turn {turn}: invalid placement")
            board[at] = stack.pop()
    remaining = sum(card >= 0 for card in board) + len(stack)
    return (n * n + 2 * n ** 3 - moves if remaining == 0 else n * n - remaining), moves


def expanded(path, ref=None):
    content = subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True) if ref else (ROOT / path).read_text()
    pattern = r'^#include "(?:../../)?library/([^"/]+)"$'
    return re.sub(pattern, lambda m: expanded("library/" + m[1], ref), content, flags=re.M)


def variants(task, reference):
    result = {"legacy": expanded(f"practice/ahc{task}/main.cpp", reference)}
    if task == "059":
        source = expanded("examples/search/ahc059_lns.cpp")
        modes = {"hill": {"MODE": 0}, "rrt2": {"MODE": 1},
                 "rrt8": {"MODE": 1, "MARGIN": 8}, "sa": {"MODE": 2},
                 "rrt2_no_cutoff": {"MODE": 1, "CUTOFF": 0},
                 "rrt2_no_precompute": {"MODE": 1, "PRECOMPUTE": 0},
                 "rrt2_no_both": {"MODE": 1, "PRECOMPUTE": 0, "CUTOFF": 0}}
        for name, settings in modes.items():
            prefix = "".join(f"#define AHC059_LNS_{key} {value}\n" for key, value in settings.items())
            result[name] = "#define AHC059_ALNS_POLICY 0\n" + prefix + source
        # 固定反復で旧LNSとの乱数列互換も検査できる。旧・入れ子鎖版にはこの機能はない。
        if "#ifdef AHC059_LNS_ITERATIONS" in result["legacy"]:
            result["previous_lns_sa"] = result["legacy"]
        for name, policy in (("uniform", 1), ("adaptive", 2)):
            result[name] = f"#define AHC059_ALNS_POLICY {policy}\n" + source
        result["adaptive_no_cutoff"] = "#define AHC059_LNS_CUTOFF 0\n" + result["adaptive"]
        result["adaptive_no_precompute"] = "#define AHC059_LNS_PRECOMPUTE 0\n" + result["adaptive"]
        result["adaptive_no_both"] = "#define AHC059_LNS_CUTOFF 0\n#define AHC059_LNS_PRECOMPUTE 0\n" + result["adaptive"]
    else:
        result["previous_sa"] = expanded("examples/search/ahc002_destroy_repair_sa.cpp", reference)
        source = expanded("examples/search/ahc002_destroy_repair_lns.cpp")
        for name, mode in (("lns_hill", 0), ("lns_rrt", 1), ("lns_sa", 2)):
            result[name] = f"#define AHC002_LNS_MODE {mode}\n" + source
    return result


def valid(row):
    return (int(row["legal"]) == 1 and int(row["over_2s"]) == 0
            and not int(row.get("diagnostic", 0)))


def summary(rows, history=()):
    best = {}
    for row in [*history, *rows]:
        if valid(row):
            key = row["input_sha256"]
            best[key] = max(best.get(key, 0), int(row["score"]))
    for version in dict.fromkeys(row["version"] for row in rows):
        selected = [r for r in rows if r["version"] == version]
        points = sum(100 * int(r["score"]) / best[r["input_sha256"]]
                     for r in selected if valid(r) and best.get(r["input_sha256"], 0) > 0)
        by_input = {}
        for r in selected:
            key = r["input_sha256"]
            value = 100 * int(r["score"]) / best[key] if valid(r) and best.get(key, 0) > 0 else 0
            by_input.setdefault(key, []).append(value)
        case_points = sum(statistics.mean(values) for values in by_input.values())
        print(f"{version}: mean_score={statistics.mean(int(r['score']) for r in selected):.3f} "
              f"relative={points:.6f}/{100 * len(selected)} average_ratio={points / len(selected):.6f}% "
              f"median_s={statistics.median(float(r['seconds']) for r in selected):.6f} "
              f"max_s={max(float(r['seconds']) for r in selected):.6f} "
              f"invalid={sum(not int(r['legal']) for r in selected)} "
              f"over_2s={sum(int(r['over_2s']) for r in selected)} "
              f"case_relative={case_points:.6f}/{100 * len(by_input)} "
              f"case_average_ratio={case_points / len(by_input):.6f}%", flush=True)
    print("Common best includes all compared versions/repeats for each input; illegal/over-2s runs earn zero. "
          "Raw means are secondary; these ratios are NOT official standings ratios.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task", choices=["059", "002"], required=True)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--reference-ref", default=BASE)
    parser.add_argument("--variants", nargs="+", required=True)
    parser.add_argument("--first-seed", type=int, default=0)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--iterations", type=int, help="AHC059 fixed-budget diagnostic; never timed adoption evidence")
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--history", type=Path, nargs="*", default=[])
    args = parser.parse_args()
    if args.first_seed < 0 or min(args.cases, args.repeats) <= 0:
        parser.error("invalid seeds/cases/repeats")
    if args.output.exists() or args.artifacts.exists():
        parser.error("choose new output/artifacts paths")
    if len(set(args.variants)) != len(args.variants):
        parser.error("duplicate variants")
    if args.iterations is not None and (args.task != "059" or args.iterations <= 0 or "legacy" in args.variants):
        parser.error("fixed iterations require AHC059 LNS variants only")
    inputs, tool, work = args.inputs.resolve(), args.tool.resolve(), args.artifacts.resolve()
    input_paths = [inputs / f"{seed:04d}.txt" for seed in range(args.first_seed, args.first_seed + args.cases)]
    if not tool.is_file() or any(not path.is_file() for path in input_paths):
        parser.error("missing official tools/inputs")
    reference = subprocess.check_output(["git", "rev-parse", "--verify", f"{args.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    sources = variants(args.task, reference)
    if any(v not in sources for v in args.variants):
        parser.error(f"available variants: {list(sources)}")
    sources = {v: sources[v] for v in args.variants}
    if args.iterations:
        sources = {v: f"#define AHC059_LNS_ITERATIONS {args.iterations}\n" + s for v, s in sources.items()}
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    work.mkdir(parents=True)
    (work / "manifest.json").write_text(json.dumps({
        "task": args.task, "reference": reference, "compiler": compiler,
        "flags": FLAGS, "inputs": str(inputs), "tool": str(tool), "tool_sha256": sha(tool.read_bytes()),
        "iterations": args.iterations, "variants": args.variants,
    }, indent=2) + "\n")
    for name, source in sources.items():
        cpp = work / f"{name}.cpp"
        cpp.write_text(source)
        subprocess.run(["g++", *FLAGS, str(cpp), "-o", str(work / name)], check=True)
    rows = []
    fields = ["task", "seed", "repeat", "version", "score", "moves", "seconds", "legal", "over_2s", "diagnostic",
              "error", "iterations", "accepted", "pruned", "input_sha256", "output_sha256", "source_sha256",
              "reference_commit", "tool_sha256", "compiler", "flags"]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for path in input_paths:
            input_text = path.read_text()
            for repeat in range(args.repeats):
                names = list(sources)
                shift = (int(path.stem) + repeat) % len(names)
                for name in names[shift:] + names[:shift]:
                    prefix = work / f"{path.stem}-{repeat}-{name}"
                    output = prefix.with_suffix(".out")
                    error, legal, score, moves = "", 0, 0, 0
                    began = time.perf_counter()
                    with output.open("w") as stdout, prefix.with_suffix(".err").open("w") as stderr:
                        try:
                            completed = subprocess.run([str(work / name)], input=input_text, text=True,
                                                       stdout=stdout, stderr=stderr, timeout=5)
                            seconds = time.perf_counter() - began
                            if completed.returncode:
                                error = f"exit {completed.returncode}"
                        except subprocess.TimeoutExpired:
                            seconds = time.perf_counter() - began
                            error = "timeout at 5s"
                    try:
                        if error: raise ValueError(error)
                        if args.task == "059":
                            score, moves = score_059(input_text, output.read_text())
                        else:
                            result = score_002(input_text, output.read_text())
                            score, moves = result.score, result.moves
                        check = subprocess.run([str(tool), str(path), str(output)], cwd=work,
                                               text=True, capture_output=True, timeout=30, check=True)
                        prefix.with_suffix(".judge").write_text(check.stdout + check.stderr)
                        official = re.fullmatch(r"Score = (\d+)\s*", check.stdout)
                        if not official or int(official[1]) != score:
                            raise ValueError("official scorer disagrees / reports invalid output")
                        legal = 1
                    except (ValueError, subprocess.SubprocessError) as exc:
                        error = str(exc)
                    logs = prefix.with_suffix(".err").read_text()
                    stats = dict(re.findall(r"(iterations|accepted|pruned)=(\d+)", logs))
                    row = dict(task=args.task, seed=int(path.stem), repeat=repeat, version=name,
                               score=score, moves=moves, seconds=seconds, legal=legal,
                               over_2s=int(seconds > 2), diagnostic=int(args.iterations is not None), error=error,
                               iterations=stats.get("iterations", ""), accepted=stats.get("accepted", ""), pruned=stats.get("pruned", ""),
                               input_sha256=sha(path.read_bytes()), output_sha256=sha(output.read_bytes()),
                               source_sha256=sha(sources[name].encode()), reference_commit=reference,
                               tool_sha256=sha(tool.read_bytes()), compiler=compiler, flags=" ".join(FLAGS))
                    rows.append(row)
                    writer.writerow(row)
                    destination.flush()
                    print(f"{args.task}/{path.stem} r{repeat} {name}: score={score} {seconds:.4f}s legal={legal} {error}", flush=True)
    history = []
    for path in args.history:
        with path.open() as source:
            history += [r for r in csv.DictReader(source) if not int(r["diagnostic"])]
    if args.iterations:
        print("FIXED-ITERATION DIAGNOSTIC: do not include in timed adoption/common-best records.")
        for name in sources:
            selected = [r for r in rows if r["version"] == name]
            print(f"{name}: mean_score={statistics.mean(r['score'] for r in selected):.3f}; "
                  "inspect output_sha256 for fixed-work equivalence (no timed relative score)")
    else:
        summary(rows, history)


if __name__ == "__main__":
    main()
