#!/usr/bin/env python3
"""Paired official AHC005 score runs with frozen single-file sources and outputs."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import time


def digest(data):
    return hashlib.sha256(data).hexdigest()


def expand(path):
    text = path.read_text()
    pattern = re.compile(r'^#include "([^"\n]+)"', re.M)
    return pattern.sub(lambda m: expand((path.parent / m[1]).resolve()), text)


def independent_score(input_path, output):
    words = input_path.read_text().split()
    n, sr, sc = map(int, words[:3])
    grid = words[3:]
    assert len(grid) == n and all(len(row) == n for row in grid)
    route = output.decode('ascii').strip()
    r, c, travel = sr, sc, 0
    seen = set()
    directions = {'U': (-1, 0), 'D': (1, 0), 'L': (0, -1), 'R': (0, 1)}
    def observe(r, c):
        seen.add((r, c))
        for dr, dc in directions.values():
            x, y = r + dr, c + dc
            while 0 <= x < n and 0 <= y < n and grid[x][y] != '#':
                seen.add((x, y))
                x, y = x + dr, y + dc
    observe(r, c)
    for ch in route:
        assert ch in directions, 'invalid move'
        dr, dc = directions[ch]
        r, c = r + dr, c + dc
        assert 0 <= r < n and 0 <= c < n and grid[r][c] != '#', 'illegal route'
        travel += int(grid[r][c])
        observe(r, c)
    assert (r, c) == (sr, sc), 'not a closed tour'
    roads = sum(ch != '#' for row in grid for ch in row)
    if len(seen) < roads:
        numerator, denominator = 10000 * len(seen), roads
        return (2 * numerator + denominator) // (2 * denominator)
    if travel == 0:
        # The official Rust scorer casts positive infinity to i64::MAX.
        return 2**63 - 1
    return 10000 + (20000000 * n + travel) // (2 * travel)



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', action='append', required=True, help='label=source.cpp')
    parser.add_argument('--tools', type=Path, required=True)
    parser.add_argument('--seeds', type=Path, required=True)
    parser.add_argument('--first-case', type=int, default=0)
    parser.add_argument('--cases', type=int, default=10)
    parser.add_argument('--repeats', type=int, default=1)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output directory already exists; never overwrite a run')
    seed_bytes = args.seeds.read_bytes()
    seeds = list(map(int, seed_bytes.split()))
    chosen = seeds[args.first_case:args.first_case + args.cases]
    if len(chosen) != args.cases or args.cases < 1 or args.repeats < 1:
        parser.error('invalid case range or repeats')
    args.output.mkdir(parents=True)
    work = args.output.resolve()
    binary_dir = args.tools.resolve() / 'target/release'
    (work / 'seeds.txt').write_text('\n'.join(map(str, chosen)) + '\n')
    subprocess.run([str(binary_dir / 'gen'), str(work / 'seeds.txt')], cwd=work, check=True, stdout=subprocess.DEVNULL)
    flags = ['-std=c++17', '-O3', '-DNDEBUG', '-Wall', '-Wextra']
    versions = {}
    for definition in args.variant:
        label, name = definition.split('=', 1)
        if not re.fullmatch(r'[a-zA-Z0-9_-]+', label) or label in versions:
            parser.error('unsafe/duplicate label')
        data = expand(Path(name).resolve()).encode()
        source = work / (label + '.cpp')
        source.write_bytes(data)
        subprocess.run(['g++', *flags, str(source), '-o', str(work / label)], check=True)
        versions[label] = digest(data)
    (work / 'manifest.json').write_text(json.dumps({
        'first_case': args.first_case, 'cases': args.cases, 'seeds': chosen,
        'repeats': args.repeats, 'flags': flags, 'source_sha256': versions,
        'seed_manifest_md5': hashlib.md5(seed_bytes).hexdigest(),
        'generator_sha256': digest((binary_dir / 'gen').read_bytes()),
        'visualizer_sha256': digest((binary_dir / 'vis').read_bytes()),
        'compiler': subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
    }, indent=2) + '\n')
    columns = ['case_index', 'seed', 'repeat', 'version', 'score', 'seconds', 'legal',
               'over_3s', 'iterations', 'pruned', 'input_sha256', 'output_sha256',
               'source_sha256', 'error']
    rows = []
    with (work / 'runs.csv').open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        for index, seed in enumerate(chosen):
            inp = work / 'in' / f'{index:04d}.txt'
            for repeat in range(args.repeats):
                labels = list(versions)
                offset = (index + repeat) % len(labels)
                labels = labels[offset:] + labels[:offset]
                for label in labels:
                    error, score, legal = '', 0, 0
                    started = time.perf_counter()
                    try:
                        completed = subprocess.run([str(work / label)], input=inp.read_bytes(),
                            capture_output=True, timeout=10, cwd=work, check=True)
                        seconds = time.perf_counter() - started
                        output, log = completed.stdout, completed.stderr
                    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as exc:
                        seconds = time.perf_counter() - started
                        output, log, error = exc.stdout or b'', exc.stderr or b'', str(exc)
                    prefix = f'{index:04d}_{repeat}_{label}'
                    answer = work / (prefix + '.out')
                    answer.write_bytes(output)
                    (work / (prefix + '.log')).write_bytes(log)
                    if not error:
                        try:
                            independent = independent_score(inp, output)
                            official = subprocess.run([str(binary_dir / 'vis'), str(inp), str(answer)],
                                capture_output=True, text=True, cwd=work, check=True)
                            assert not official.stderr.strip(), official.stderr
                            match = re.fullmatch(r'Score = (\d+)\s*', official.stdout)
                            assert match, official.stdout
                            score = int(match[1])
                            assert score == independent, (score, independent)
                            legal = 1
                        except (AssertionError, ValueError, subprocess.CalledProcessError) as exc:
                            error = str(exc)
                    stats = dict(re.findall(r'(iterations|pruned)=(\d+)', log.decode(errors='replace')))
                    row = dict(case_index=args.first_case + index, seed=seed, repeat=repeat,
                        version=label, score=score, seconds=seconds, legal=legal,
                        over_3s=int(seconds > 3), iterations=stats.get('iterations', ''),
                        pruned=stats.get('pruned', ''), input_sha256=digest(inp.read_bytes()),
                        output_sha256=digest(output), source_sha256=versions[label], error=error)
                    writer.writerow(row)
                    stream.flush()
                    rows.append(row)
                    print(f'case={args.first_case + index} repeat={repeat} version={label} '
                          f'score={score} seconds={seconds:.3f} legal={legal}', flush=True)
    best = {seed: max([r['score'] for r in rows if r['seed'] == seed and r['legal']
                      and not r['over_3s']] or [1]) for seed in chosen}
    summary = {}
    for label in versions:
        selected = [r for r in rows if r['version'] == label]
        valid_score = lambda r: r['score'] if r['legal'] and not r['over_3s'] else 0
        relative = sum(100 * valid_score(r) / best[r['seed']] for r in selected) / args.repeats
        summary[label] = dict(relative=relative, average_percent=relative / args.cases,
            mean_score=sum(valid_score(r) for r in selected) / len(selected),
            failures=sum(not r['legal'] or r['over_3s'] for r in selected),
            max_seconds=max(r['seconds'] for r in selected))
    (work / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == '__main__':
    main()
