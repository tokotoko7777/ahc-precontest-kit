#!/usr/bin/env python3
"""AHC006: full-copy vs true delta SA; official and independent scoring.

Compile before sequential rotated-order runs. --iterations is a deterministic
trajectory diagnostic, never score/adoption evidence. Keep all failures in CSV.
"""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess
import time
from lns_official_benchmark import ROOT, FLAGS, expanded, sha, summary

BASE = "9eb7c90011bec7c5bc8cde4e7447d78af295e2ac"


def score_output(text, output):
    data = list(map(int, text.split()))
    if len(data) != 4000 or any(x < 0 or x > 800 for x in data):
        raise ValueError("invalid input")
    numbers = list(map(int, output.split()))
    if len(numbers) < 52 or numbers[0] != 50:
        raise ValueError("must choose 50 orders")
    orders, n = numbers[1:51], numbers[51]
    if len(set(orders)) != 50 or any(not 1 <= i <= 1000 for i in orders):
        raise ValueError("invalid/duplicate order")
    if n < 2 or len(numbers) != 52 + 2 * n:
        raise ValueError("invalid route length / extra output")
    route = list(zip(numbers[52::2], numbers[53::2]))
    if route[0] != (400, 400) or route[-1] != (400, 400):
        raise ValueError("invalid depot")
    if any(not 0 <= x <= 800 or not 0 <= y <= 800 for x, y in route):
        raise ValueError("outside coordinate range")
    first, last = {}, {}
    for i, point in enumerate(route):
        first.setdefault(point, i)
        last[point] = i
    for order in orders:
        x, y, xx, yy = data[4 * (order - 1):4 * order]
        if first.get((x, y), n) >= last.get((xx, yy), -1):
            raise ValueError("pickup/delivery missing or out of order")
    cost = sum(abs(x - xx) + abs(y - yy) for (x, y), (xx, yy) in zip(route, route[1:]))
    return int(1e8 / (1000 + cost) + 0.5), cost


def fixed_driver(iterations):
    # Same RNG draw order, prescribed progress and temperature for both Problems.
    # Hash every valid/rejected proposal, delta and applied route, not only best.
    return r'''
int main() {
  DeliveryProblem problem; problem.read_input();
  auto state = problem.make_initial_state(), best = state;
  mt19937_64 moves(20211115ULL ^ 0xd1b54a32d192ed03ULL), accept(20211115);
  uint64_t trace = 1469598103934665603ULL, accepted = 0;
  const auto hash = [&](uint64_t x) { trace = (trace ^ x) * 1099511628211ULL; };
  for (int i = 0; i < ITERATION_COUNT; ++i) {
    const double progress = double(i) / ITERATION_COUNT;
    const auto move = problem.propose_move(state, moves, progress);
    hash(bool(move));
    if (!move) continue;
    const auto improvement = problem.evaluate_move(state, *move, -INFINITY);
    if (!improvement) return 3;
    hash(*improvement);
    const double temperature = exp(log(120.0) * (1.0 - progress));
    const bool take = *improvement >= 0 ||
        double(accept() >> 11) * 0x1.0p-53 < exp(double(*improvement) / temperature);
    hash(take);
    if (take) {
      ++accepted; problem.apply_move(state, *move);
      if (state.cost < best.cost) best = state;
    }
    hash(state.cost);
    for (int event : state.route) hash(event);
  }
  print_answer(problem, best);
  cerr << "iterations=" << ITERATION_COUNT << " accepted=" << accepted
       << " trace=" << trace << '\n';
}
'''.replace("ITERATION_COUNT", str(iterations))


def sources(ref, iterations=None):
    old = expanded("examples/search/ahc006_sa.cpp", ref)
    current = expanded("examples/search/ahc006_sa.cpp")
    if iterations:
        old = old[:old.rindex("int main() {")] + fixed_driver(iterations)
        current = current[:current.rindex("#ifndef AHC006_NO_MAIN")] + fixed_driver(iterations)
    else:
        # Normalize old Runner to the same end-to-end 1850ms including initialization.
        old = old.replace("int main() {", "int main() {\n  const auto start = chrono::steady_clock::now();")
        old = old.replace("      SEARCH_EXAMPLE_TIME_LIMIT_MS,", "      max(0.001, SEARCH_EXAMPLE_TIME_LIMIT_MS - chrono::duration<double, milli>(chrono::steady_clock::now() - start).count()),")
        old = old.replace("  print_answer(problem, answer);", '  cerr << "iterations=" << runner.iterations() << " accepted=" << runner.accepted_moves() << "\\n";\n  print_answer(problem, answer);')
    result = {"full_copy": old, "delta": "#define AHC006_STATS\n#define AHC006_PRECOMPUTE_DISTANCE 0\n" + current,
              "delta_table": "#define AHC006_STATS\n#define AHC006_PRECOMPUTE_DISTANCE 1\n" + current}
    if not iterations:
        result["legacy_practice"] = expanded("practice/ahc006/main.cpp", ref)
    # 同じ差分SAのままpragmaだけを切り替える。通常の比較版名とは分けて記録。
    result["gcc_o3"] = result["delta_table"]
    result["gcc_disabled"] = "#define AHC_DISABLE_GCC_OPTIMIZE\n" + result["delta_table"]
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--tool", type=Path, required=True)
    p.add_argument("--reference-ref", default=BASE)
    p.add_argument("--variants", nargs="+", default=["full_copy", "delta", "delta_table"])
    p.add_argument("--cases", type=int, default=10)
    p.add_argument("--first-seed", type=int, default=0)
    p.add_argument("--repeats", type=int, default=2)
    p.add_argument("--iterations", type=int)
    p.add_argument("--artifacts", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--history", nargs="*", type=Path, default=[])
    a = p.parse_args()
    if a.output.exists() or a.artifacts.exists(): p.error("choose unused output/artifacts")
    if min(a.cases, a.repeats) <= 0 or a.first_seed < 0 or (a.iterations is not None and a.iterations <= 0): p.error("invalid budgets")
    if len(set(a.variants)) != len(a.variants) or not a.variants: p.error("invalid variants")
    inputs, tool, work = a.inputs.resolve(), a.tool.resolve(), a.artifacts.resolve()
    paths = [inputs / f"{s:04d}.txt" for s in range(a.first_seed, a.first_seed + a.cases)]
    if not tool.is_file() or any(not path.is_file() for path in paths): p.error("missing inputs/tool")
    ref = subprocess.check_output(["git", "rev-parse", f"{a.reference_ref}^{{commit}}"], cwd=ROOT, text=True).strip()
    available = sources(ref, a.iterations)
    if any(v not in available for v in a.variants): p.error(f"available: {list(available)}")
    versions = {v: available[v] for v in a.variants}
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    work.mkdir(parents=True)
    (work / "manifest.json").write_text(json.dumps(dict(reference=ref, compiler=compiler, flags=FLAGS,
        tool_sha256=sha(tool.read_bytes()), inputs=str(inputs), variants=a.variants, iterations=a.iterations), indent=2) + "\n")
    for name, source in versions.items():
        cpp = work / (name + ".cpp"); cpp.write_text(source)
        subprocess.run(["g++", *FLAGS, str(cpp), "-o", str(work / name)], check=True)
    fields = ["task", "seed", "repeat", "version", "score", "moves", "seconds", "legal", "over_2s", "diagnostic", "error",
              "iterations", "accepted", "pruned", "input_sha256", "output_sha256", "source_sha256", "reference_commit", "tool_sha256", "compiler", "flags"]
    rows, traces = [], {}
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open("x", newline="") as dest:
        writer = csv.DictWriter(dest, fieldnames=fields, lineterminator="\n"); writer.writeheader()
        for path in paths:
            text = path.read_text()
            for repeat in range(a.repeats):
                names = list(versions); offset = (int(path.stem) + repeat) % len(names)
                for name in names[offset:] + names[:offset]:
                    prefix = work / f"{path.stem}-{repeat}-{name}"
                    output, err = prefix.with_suffix(".out"), prefix.with_suffix(".err")
                    score = cost = legal = 0; error = ""; start = time.perf_counter()
                    try:
                        with output.open("w") as out, err.open("w") as stderr:
                            subprocess.run([str(work / name)], input=text, text=True, stdout=out, stderr=stderr, timeout=30 if a.iterations else 5, check=True)
                        seconds = time.perf_counter() - start
                    except (OSError, subprocess.SubprocessError) as exc:
                        seconds = time.perf_counter() - start; error = str(exc)
                    try:
                        if error: raise ValueError(error)
                        score, cost = score_output(text, output.read_text())
                        judge = subprocess.run([str(tool), str(path), str(output)], cwd=work, text=True, capture_output=True, timeout=30, check=True)
                        prefix.with_suffix(".judge").write_text(judge.stdout + judge.stderr)
                        match = re.fullmatch(r"Score = (\d+)\s*", judge.stdout)
                        if not match or int(match[1]) != score: raise ValueError("official/independent score mismatch")
                        legal = 1
                    except (ValueError, OSError, subprocess.SubprocessError) as exc: error = str(exc)
                    stats = dict(re.findall(r"(iterations|accepted|trace)=(\d+)", err.read_text() if err.exists() else ""))
                    if a.iterations:
                        traces.setdefault((path.stem, repeat), []).append((legal, stats.get("trace"), sha(output.read_bytes())))
                    row = dict(task="006", seed=int(path.stem), repeat=repeat, version=name, score=score, moves=cost, seconds=seconds,
                        legal=legal, over_2s=int(seconds > 2), diagnostic=int(bool(a.iterations)), error=error,
                        iterations=stats.get("iterations", ""), accepted=stats.get("accepted", ""), pruned="",
                        input_sha256=sha(path.read_bytes()), output_sha256=sha(output.read_bytes()) if output.exists() else "",
                        source_sha256=sha(versions[name].encode()), reference_commit=ref, tool_sha256=sha(tool.read_bytes()), compiler=compiler, flags=" ".join(FLAGS))
                    rows.append(row); writer.writerow(row); dest.flush()
                    print(f"006/{path.stem} {name} r{repeat}: {score} {seconds:.4f}s legal={legal} {error}", flush=True)
    if a.iterations:
        if any(len(set(v)) != 1 or v[0][0] != 1 or v[0][1] is None for v in traces.values()):
            raise SystemExit("fixed-iteration trajectory mismatch")
        print("Every per-move trajectory hash and final answer matches (diagnostic, not adoption evidence).")
    else:
        history = []
        for path in a.history:
            with path.open() as src: history.extend(csv.DictReader(src))
        summary(rows, history)


if __name__ == "__main__": main()
