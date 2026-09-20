#!/usr/bin/env python3
"""Validate frozen sources, raw results, and the common-denominator report."""
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / 'benchmarks/ahc004_five_rounds'
spec = importlib.util.spec_from_file_location(
    'report', ROOT / 'benchmarks/ahc004_improvement_report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)

assert report.score({'score': '10', 'legal': '1', 'over_3s': '0'}) == 10
for overrides in [{'legal': '0'}, {'over_3s': '1'}, {'diagnostic': '1'}]:
    assert report.score(dict(score='10', legal='1', over_3s='0') | overrides) == 0

with tempfile.TemporaryDirectory(prefix='ahc004-snapshot-test-') as directory:
    directory = Path(directory)
    for round_id in range(1, 6):
        path = directory / f'round{round_id}.cpp'
        subprocess.run(['patch', '--silent', '--output', str(path),
                        str(ARCHIVE / 'baseline.cpp'),
                        str(ARCHIVE / f'round{round_id}.patch')], check=True)
        manifest = json.loads((ARCHIVE / f'round{round_id}.manifest.json').read_text())
        expected = manifest['source_sha256'][f'round{round_id}']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == expected

actual = json.loads(subprocess.check_output([
    sys.executable, str(ROOT / 'benchmarks/ahc004_improvement_report.py'),
    '--experiment', str(ROOT / 'benchmarks/results'),
    '--history', str(ROOT / 'benchmarks/results')], text=True))
saved = json.loads((ARCHIVE / 'final-summary.json').read_text())
# 過去のreportはその時点の共通分母で検査する。将来の最高点追加は失敗理由にしない。
for round_id in range(1, 6):
    data = report.rows(ROOT / f'benchmarks/results/ahc004-five-rounds-{round_id}-dev.csv')
    key = f'round{round_id}-dev'
    assert report.summary(data, saved['best_by_input']) == saved['rounds'][key]
    assert report.summary(data, actual['best_by_input']) == actual['rounds'][key]
data = report.rows(ROOT / 'benchmarks/results/ahc004-five-rounds-confirmation.csv')
assert report.summary(data, saved['confirmation_best_by_input']) == saved['confirmation']
assert report.summary(data, actual['confirmation_best_by_input']) == actual['confirmation']
print('AHC004 improvement records passed: five source snapshots + shared-denominator scores')

selection = json.loads((ARCHIVE / 'selection.json').read_text())
confirmation = json.loads((ARCHIVE / 'confirmation.manifest.json').read_text())
assert selection['recorded_before_confirmation']
assert confirmation['source_sha256'][selection['selected']] == selection['source_sha256']
assert confirmation['seeds'] == list(range(20, 40))


# The scorer is intentionally independent of the automaton and packed matching.
spec = importlib.util.spec_from_file_location(
    'bench', ROOT / 'benchmarks/ahc004_improvement_benchmark.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)
with tempfile.TemporaryDirectory(prefix='ahc004-score-test-') as directory:
    inp = Path(directory) / 'input.txt'
    inp.write_text('20 3\nAB\nAB\nBA\n')
    board = ['.' * 20 for _ in range(20)]
    board[0] = 'B' + '.' * 18 + 'A'  # AB only across the cyclic boundary
    output = ('\n'.join(board) + '\n').encode()
    assert bench.independent_score(inp, output) == 66666667
    board[0] = 'BA' + '.' * 17 + 'A'  # AB and BA, 397 empty cells
    output = ('\n'.join(board) + '\n').encode()
    assert bench.independent_score(inp, output) == (160000000000 + 403) // 806
    empty = (('.' * 20 + '\n') * 20).encode()
    assert bench.independent_score(inp, empty) == 0  # legal zero, not a crash
    for invalid in [b'', b'x', empty.replace(b'.', b'X', 1), empty + b'EXTRA']:
        try:
            bench.independent_score(inp, invalid)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('invalid output accepted')
legal_zero = dict(score='0', legal='1', over_3s='0', version='v', input_sha256='x')
assert report.summary([legal_zero], {'x': 1})['v']['failures'] == 0
print('AHC004 independent scorer passed: wrap, duplicates, bonus, zero, invalid output')
