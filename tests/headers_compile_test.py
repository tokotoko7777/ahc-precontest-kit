#!/usr/bin/env python3
"""Compile library headers independently and together without changing them."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
headers = sorted((root / 'library').glob('*.hpp'))
if not headers:
    raise SystemExit('No library headers found')
compiler = os.environ.get('CXX', 'g++')
with tempfile.TemporaryDirectory(prefix='ahc-header-check-') as directory:
    source = Path(directory) / 'check.cpp'
    groups = [[header] for header in headers] + [headers]
    for group in groups:
        source.write_text(''.join(f'#include "{p.relative_to(root)}"\n' for p in group) + 'int main() { return 0; }\n')
        result = subprocess.run([compiler, '-std=c++17', '-O2', '-I', str(root),
                                 '-fsyntax-only', str(source)], capture_output=True, text=True)
        if result.returncode:
            log = root / 'build' / 'header-check.log'
            log.parent.mkdir(exist_ok=True)
            log.write_text(result.stdout + result.stderr)
            label = group[0].name if len(group) == 1 else 'all headers together'
            raise SystemExit(f'Compilation failed: {label}; details in {log}')
print(f'Passed: {len(headers)} individual headers and one combined translation unit')
