#!/usr/bin/env python3
"""Recompute all rounds against the same latest per-input historical best vector."""
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def rows(path):
    with path.open() as source:
        return list(csv.DictReader(source))


def score(row):
    if (row.get('legal', '1') != '1' or row.get('over_5s') != '0'
            or row.get('diagnostic', '0') != '0'):
        return 0
    return int(row['score'])


def summary(data, best):
    by_version = defaultdict(lambda: defaultdict(list))
    for row in data:
        by_version[row['version']][row['input_sha256']].append(row)
    result = {}
    for version, cases in by_version.items():
        relative = sum(sum(100 * score(r) / best[key] for r in runs) / len(runs)
                       for key, runs in cases.items())
        mean_score = sum(sum(score(r) for r in runs) / len(runs)
                         for runs in cases.values()) / len(cases)
        result[version] = dict(relative=relative, average_percent=relative / len(cases),
            mean_score=mean_score, cases=len(cases), runs=sum(map(len, cases.values())),
            failures=sum(not score(r) for runs in cases.values() for r in runs))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--experiment', type=Path, required=True)
    parser.add_argument('--history', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    best = defaultdict(lambda: 1)
    historical = sorted(args.history.glob('ahc001*.csv'))
    round_files = sorted(p for p in args.experiment.glob('round*-dev/runs.csv')
                         if (p.parent / 'summary.json').is_file())
    if not round_files:
        round_files = sorted(args.experiment.glob('ahc001-five-rounds-*-dev.csv'))
    for path in [*historical, *round_files]:
        for row in rows(path):
            key = row.get('input_sha256')
            if key:
                best[key] = max(best[key], score(row))
    output = {'rounds': {}, 'historical_files': [p.name for p in historical]}
    combined = []
    for path in round_files:
        data = rows(path)
        combined.extend(data)
        label = (path.parent.name if path.name == 'runs.csv' else
                 'round' + path.name.removeprefix('ahc001-five-rounds-').removesuffix('-dev.csv') + '-dev')
        output['rounds'][label] = summary(data, best)
    output['all_development'] = summary(combined, best)
    keys = {r['input_sha256'] for r in combined}
    output['best_by_input'] = {k: best[k] for k in sorted(keys)}
    confirmation = args.experiment / 'confirmation/runs.csv'
    if not confirmation.exists():
        confirmation = args.experiment / 'ahc001-five-rounds-confirmation.csv'
    if confirmation.exists() and (confirmation.name != 'runs.csv' or
                                  (confirmation.parent / 'summary.json').is_file()):
        data = rows(confirmation)
        for row in data:
            best[row['input_sha256']] = max(best[row['input_sha256']], score(row))
        output['confirmation'] = summary(data, best)
        output['confirmation_best_by_input'] = {
            row['input_sha256']: best[row['input_sha256']] for row in data}
    rendered = json.dumps(output, indent=2) + '\n'
    if args.output:
        with args.output.open('x') as destination:
            destination.write(rendered)
    print(rendered)


if __name__ == '__main__':
    main()
