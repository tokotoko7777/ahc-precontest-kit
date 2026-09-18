#!/usr/bin/env python3
"""Keep O3 before all copied definitions, without unroll/Ofast additions."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from copy_part import GCC_OPTIMIZATION_PROLOGUE, FrozenPart, render_parts


def main():
    paths = sorted([*ROOT.glob("template/**/*.cpp"), *ROOT.glob("examples/**/*.cpp"),
                    *ROOT.glob("practice/ahc*/main.cpp"), *ROOT.glob("benchmarks/*.cpp")])
    excluded = ROOT / "practice/ahc069/main.cpp"  # 現行コンテストは変更対象外。
    checked = 0
    for path in paths:
        if path == excluded:
            continue
        data = path.read_bytes()
        assert data.startswith(GCC_OPTIMIZATION_PROLOGUE), path
        assert data.count(b"#pragma GCC optimize") == 1, path
        assert b'optimize("O3")' in data, path
        assert b"unroll-loops" not in data and b"Ofast" not in data, path
        checked += 1
    part = FrozenPart("library/test.hpp", b"int helper() { return 42; }\n", "test", "https://example.com/test")
    main_source = GCC_OPTIMIZATION_PROLOGUE + b"int main() { return helper() - 42; }\n"
    for source in (None, main_source, b"int main() { return helper() - 42; }\n"):
        generated = render_parts([part], source)
        assert generated.startswith(GCC_OPTIMIZATION_PROLOGUE)
        assert generated.count(b"#pragma GCC optimize") == 1
        assert part.content in generated  # URL/hash対象のhpp内容はそのまま。
    # GCCでON/OFFとClangガードを前処理検査。ソルバ実行はしない。
    cxx = os.environ.get("CXX", "g++")
    for flags, enabled in (([], True), (["-DAHC_DISABLE_GCC_OPTIMIZE"], False),
                           (["-D__clang__"], False)):
        out = subprocess.check_output([cxx, "-E", "-P", "-x", "c++", *flags, "-"],
                                      input=GCC_OPTIMIZATION_PROLOGUE)
        assert (b'#pragma GCC optimize("O3")' in out) == enabled
    print(f"GCC O3 prologue test passed: {checked} C++ files + copied/offline output")


if __name__ == "__main__":
    main()
