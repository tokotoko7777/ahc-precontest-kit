"""Keep search families discoverable without limiting their variants."""
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    basic = ROOT / "template/search"
    required = {
        "beam": {"simple.cpp", "action.cpp", "action-options.cpp", "tree.cpp",
                 "variable-cost-tree.cpp", "chokudai.cpp", "multi-start.cpp"},
        "local-search": {"basic.cpp", "annealing-options.cpp", "destroy-repair.cpp",
                         "adaptive-destroy-repair.cpp", "iterated-local-search.cpp",
                         "prefix-replay.cpp", "forward-backward.cpp"},
        "monte-carlo": {"rollout.cpp", "tree-search.cpp"},
    }
    assert not list(basic.glob("*.cpp")), "put variants inside their family folder"
    for family, names in required.items():
        actual = {p.name for p in (basic / family).glob("*.cpp")}
        assert names <= actual, (family, names - actual)
    # Additional variants are welcome; each must be listed in its family's index.
    for family in (p for p in basic.iterdir() if p.is_dir()):
        index = family / "README.md"
        assert index.is_file(), family
        for source in family.glob("*.cpp"):
            assert f"]({source.name})" in index.read_text(), source
    advanced = ROOT / "template/advanced"
    assert (advanced / "deterministic-rollout.cpp").is_file()

    local = (basic / "local-search/basic.cpp").read_text()
    selector = "constexpr bool USE_ANNEALING = true;"
    assert selector in local and "if constexpr (USE_ANNEALING)" in local
    assert "run_with_threshold()" in local and "run_hill_climbing()" in local
    assert local.count("struct Problem {") == 1
    assert "PRECOMPUTE_ACCEPTANCE" not in local
    # Both choices must compile with the same problem-side code, not just the default.
    for mode in ("true", "false"):
        source = local.replace(selector, f"constexpr bool USE_ANNEALING = {mode};")
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-O3",
                        "-Wall", "-Wextra", "-Werror", "-I", str(ROOT),
                        "-x", "c++", "-fsyntax-only", "-"],
                       input=source, text=True, check=True)
    rebuild = (basic / "local-search/destroy-repair.cpp").read_text()
    assert "options.acceptance = LnsAcceptance::HillClimbing;" in rebuild
    assert "同点も採用" in rebuild
    action = (basic / "beam/action.cpp").read_text()
    assert "evaluate_action_with_threshold" not in action and "enumerate_actions" not in action
    for path in [*basic.rglob("*.cpp"), *advanced.glob("*.cpp")]:
        source = path.read_text()
        assert "TODO:" in source, path
        for name in re.findall(r'#include "(library/[^"]+)"', source):
            assert (ROOT / name).is_file(), (path, name)

    make = (ROOT / "Makefile").read_text()
    patterns = re.search(r"^SEARCH_STARTERS := \$\(wildcard (.+)\)$", make, re.M)[1].split()
    compiled = {p for pattern in patterns for p in ROOT.glob(pattern)}
    assert compiled == set(basic.rglob("*.cpp")) | set(advanced.glob("*.cpp"))

    # Every relative link from the entry docs, and every template link in other docs.
    entry_docs = {ROOT / p for p in ["README.md", "SEARCH_GUIDE.md", "SEARCH_REFERENCE.md",
        "ALGORITHM_SELECTION.md", "template/advanced/README.md", "examples/search/README.md"]}
    entry_docs.update(basic.rglob("README.md"))
    for path in ROOT.rglob("*.md"):
        if any(part in {".git", "build"} for part in path.relative_to(ROOT).parts):
            continue
        content = path.read_text()
        for target in re.findall(r"\]\(([^\s)]+)\)", content):
            if re.match(r"(?:https?://|#|mailto:)", target):
                continue
            if path not in entry_docs and "template/" not in target:
                continue
            target = target.split("#", 1)[0]
            assert (path.parent / target).exists(), (path, target)
    examples = (ROOT / "examples/search/README.md").read_text()
    listed = re.findall(r"^\| \[\x60([^\x60]+\.cpp)\x60\]", examples, re.M)
    assert len(listed) == len(set(listed)), "duplicated examples"
    assert set(listed) == {p.name for p in (ROOT / "examples/search").glob("*.cpp")}
    headings = re.findall(r"^#{1,3} .+$", (ROOT / "README.md").read_text(), re.M)
    assert len(headings) == len(set(headings)), "duplicated README sections"
    print(f"search layout passed: {len(required)} families, {len(compiled)} templates, both local-search modes")


if __name__ == "__main__":
    main()
