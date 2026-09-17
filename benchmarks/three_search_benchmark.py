#!/usr/bin/env python3
"""Official-score comparisons for chokudai (032), UCT (015), and ILS (059).

Build every version first; run sequentially in rotated order. AHC015 is genuinely
interactive: do not send the next placement before reading the current action.
Reuse the LNS CSV schema and common-best aggregation (all versions/all repeats).
"""
import argparse
import csv
import json
from pathlib import Path
import re
import selectors
import subprocess
import time
from lns_official_benchmark import ROOT, FLAGS, expanded, sha, summary, score_059
from ahc032_core_benchmark import replay_score as score_032

BASE = "a2939214735b5b70a220955f19c62909b3cf8d0d"


def sources(task, ref):
    result = {"legacy": expanded(f"practice/ahc{task}/main.cpp", ref)}
    if task == "032":
        for width in (6000, 9000, 12000):
            result[f"beam{width}"] = f"#define AHC032_BEAM_WIDTH {width}\n" + result["legacy"]
        src = expanded("examples/search/ahc032_chokudai.cpp")
        for cap in (64, 256, 1024):
            result[f"chokudai{cap}"] = f"#define AHC032_CHOKUDAI_CAPACITY {cap}\n" + src
    elif task == "015":
        src = expanded("examples/search/ahc015_uct.cpp")
        result["flat"] = "#define AHC015_USE_UCT 0\n" + src
        for depth in (1, 8, 32):
            result[f"uct{depth}"] = f"#define AHC015_UCT_DEPTH {depth}\n" + src
        result["uct8_ordered"] = "#define AHC015_ORDER_ACTIONS 1\n" + result["uct8"]
        result["uct8_low"] = "#define AHC015_UCT_EXPLORATION 0.05\n" + result["uct8_ordered"]
        result["uct1_low"] = "#define AHC015_UCT_EXPLORATION 0.05\n#define AHC015_ORDER_ACTIONS 1\n" + result["uct1"]
    else:
        src = expanded("examples/search/ahc059_ils.cpp")
        result["ils64"] = src
        result["ils16"] = "#define AHC059_ILS_STALL 16\n" + src
        result["ils_walk"] = "#define AHC059_ILS_WALK 1\n" + src
    return result


def candy_input(text):
    values = list(map(int, text.split()))
    if len(values) != 200 or any(f not in (1, 2, 3) for f in values[:100]):
        raise ValueError("invalid AHC015 input")
    if any(not 1 <= r <= 100 - t for t, r in enumerate(values[100:])):
        raise ValueError("invalid AHC015 rank")
    return values[:100], values[100:]


def score_015(text, output):
    flavors, ranks = candy_input(text)
    moves = output.split()
    if len(moves) not in (99, 100) or any(m not in "FBLR" or len(m) != 1 for m in moves):
        raise ValueError("invalid AHC015 output")
    board = [0] * 100
    for t, rank in enumerate(ranks):
        empty = [i for i, value in enumerate(board) if not value]
        board[empty[rank - 1]] = flavors[t]
        direction = moves[t] if t < len(moves) else "F"
        for line in range(10):
            cells = [j * 10 + line for j in range(10)] if direction in "FB" else [line * 10 + j for j in range(10)]
            if direction in "BR": cells.reverse()
            candies = [board[i] for i in cells if board[i]]
            for j, i in enumerate(cells): board[i] = candies[j] if j < len(candies) else 0
    visited = set()
    numerator = 0
    for i in range(100):
        if i in visited: continue
        stack = [i]; visited.add(i); size = 0
        while stack:
            v = stack.pop(); size += 1
            y, x = divmod(v, 10)
            for yy, xx in ((y-1,x), (y+1,x), (y,x-1), (y,x+1)):
                u = yy * 10 + xx
                if 0 <= yy < 10 and 0 <= xx < 10 and u not in visited and board[u] == board[v]:
                    visited.add(u); stack.append(u)
        numerator += size * size
    denominator = sum(flavors.count(f) ** 2 for f in (1, 2, 3))
    return int(1e6 * numerator / denominator + 0.5)


def interact(binary, text, output, error, timeout=5):
    flavors, ranks = candy_input(text)
    start = time.perf_counter()
    with error.open("w") as err, output.open("w") as out:
        command = binary if isinstance(binary, list) else [str(binary)]
        proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err, bufsize=0)
        selector = selectors.DefaultSelector()
        selector.register(proc.stdout, selectors.EVENT_READ)
        pending = b""
        try:
            proc.stdin.write((" ".join(map(str, flavors)) + "\n").encode())
            for t, rank in enumerate(ranks):
                proc.stdin.write(f"{rank}\n".encode())
                while b"\n" not in pending:
                    left = timeout - (time.perf_counter() - start)
                    if left <= 0 or not selector.select(left): raise TimeoutError("interactive timeout")
                    data = proc.stdout.read(4096)
                    if not data: raise ValueError(f"EOF before action {t}")
                    pending += data
                line, pending = pending.split(b"\n", 1)
                action = line.decode().strip()
                if action not in ("F", "B", "L", "R"): raise ValueError(f"invalid action {t}")
                out.write(action + "\n")
            proc.stdin.close()
            proc.wait(timeout=max(0.01, timeout - (time.perf_counter() - start)))
            if pending.strip() or proc.stdout.read().strip(): raise ValueError("extra output")
            if proc.returncode: raise ValueError(f"exit {proc.returncode}")
        finally:
            selector.close()
            if proc.poll() is None: proc.kill()
            proc.wait()
            if not proc.stdin.closed: proc.stdin.close()
            proc.stdout.close()
    return time.perf_counter() - start


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--task", choices=("032", "015", "059"), required=True)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--tool", type=Path, required=True)
    p.add_argument("--reference-ref", default=BASE)
    p.add_argument("--variants", nargs="+", required=True)
    p.add_argument("--cases", type=int, default=5)
    p.add_argument("--first-seed", type=int, default=0)
    p.add_argument("--repeats", type=int, default=1)
    p.add_argument("--artifacts", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--history", nargs="*", type=Path, default=[])
    a = p.parse_args()
    if a.output.exists() or a.artifacts.exists(): p.error("choose unused output/artifacts")
    if min(a.cases,a.repeats) <= 0 or a.first_seed < 0 or len(set(a.variants)) != len(a.variants): p.error("invalid cases/repeats/variants")
    inputs, tool, work = a.inputs.resolve(), a.tool.resolve(), a.artifacts.resolve()
    paths = [inputs / f"{s:04d}.txt" for s in range(a.first_seed,a.first_seed+a.cases)]
    if not tool.is_file() or any(not path.is_file() for path in paths): p.error("missing inputs/tool")
    ref = subprocess.check_output(["git","rev-parse",f"{a.reference_ref}^{{commit}}"],cwd=ROOT,text=True).strip()
    available = sources(a.task,ref)
    if any(v not in available for v in a.variants): p.error(f"available: {list(available)}")
    versions = {v:available[v] for v in a.variants}
    compiler = subprocess.check_output(["g++","--version"],text=True).splitlines()[0]
    work.mkdir(parents=True)
    (work / "manifest.json").write_text(json.dumps(dict(task=a.task,reference=ref,compiler=compiler,flags=FLAGS,
        tool_sha256=sha(tool.read_bytes()),inputs=str(inputs),variants=a.variants,interactive=a.task=="015"),indent=2)+"\n")
    for name, source in versions.items():
        cpp = work / (name+".cpp"); cpp.write_text(source)
        subprocess.run(["g++",*FLAGS,str(cpp),"-o",str(work/name)],check=True)
    rows=[]
    fields=["task","seed","repeat","version","score","moves","seconds","legal","over_2s","diagnostic","error",
            "iterations","accepted","pruned","input_sha256","output_sha256","source_sha256","reference_commit","tool_sha256","compiler","flags"]
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open("x",newline="") as dest:
        writer=csv.DictWriter(dest,fieldnames=fields,lineterminator="\n");writer.writeheader()
        for path in paths:
            text=path.read_text()
            for repeat in range(a.repeats):
                names=list(versions); offset=(int(path.stem)+repeat)%len(names)
                for name in names[offset:]+names[:offset]:
                    prefix=work/f"{path.stem}-{repeat}-{name}"
                    output=prefix.with_suffix(".out"); err=prefix.with_suffix(".err")
                    score=moves=legal=0; error=""; start=time.perf_counter()
                    try:
                        if a.task=="015": seconds=interact(work/name,text,output,err)
                        else:
                            with output.open("w") as out,err.open("w") as stderr:
                                subprocess.run([str(work/name)],input=text,text=True,stdout=out,stderr=stderr,timeout=5,check=True)
                            seconds=time.perf_counter()-start
                    except (OSError,ValueError,TimeoutError,subprocess.SubprocessError) as exc:
                        seconds=time.perf_counter()-start;error=str(exc)
                    try:
                        if error: raise ValueError(error)
                        if a.task=="015": score=score_015(text,output.read_text())
                        elif a.task=="032": score=score_032(path,output)
                        else: score,moves=score_059(text,output.read_text())
                        official=subprocess.run([str(tool),str(path),str(output)],cwd=work,text=True,capture_output=True,timeout=30,check=True)
                        prefix.with_suffix(".judge").write_text(official.stdout+official.stderr)
                        match=re.fullmatch(r"Score = (\d+)\s*",official.stdout)
                        if not match or int(match[1])!=score: raise ValueError("official/independent score mismatch")
                        legal=1
                    except (ValueError,OSError,subprocess.SubprocessError) as exc: error=str(exc)
                    log=err.read_text() if err.exists() else ""
                    stats=dict(re.findall(r"(iterations|accepted|pruned)=(\d+)",log))
                    row=dict(task=a.task,seed=int(path.stem),repeat=repeat,version=name,score=score,moves=moves,seconds=seconds,
                        legal=legal,over_2s=int(seconds>2),diagnostic=0,error=error,iterations=stats.get("iterations",""),
                        accepted=stats.get("accepted",""),pruned=stats.get("pruned",""),input_sha256=sha(path.read_bytes()),
                        output_sha256=sha(output.read_bytes()) if output.exists() else "",source_sha256=sha(versions[name].encode()),
                        reference_commit=ref,tool_sha256=sha(tool.read_bytes()),compiler=compiler,flags=" ".join(FLAGS))
                    rows.append(row);writer.writerow(row);dest.flush()
                    print(f"{a.task}/{path.stem} {name} r{repeat}: {score} {seconds:.4f}s legal={legal} {error}",flush=True)
    history=[]
    for path in a.history:
        with path.open() as src: history.extend(csv.DictReader(src))
    summary(rows,history)


if __name__=="__main__": main()
