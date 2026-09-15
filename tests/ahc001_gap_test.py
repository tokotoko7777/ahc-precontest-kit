#!/usr/bin/env python3
import importlib.util
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("gap_report", ROOT / "benchmarks/ahc001_gap_report.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

for suite in ("provisional", "system"):
    reference = module.REFERENCE[suite]
    report = module.gap(reference["score"], reference["cases"], suite)
    assert report["ratio_percent"] == 100
    assert report["mean_gap"] == 0
    assert report["loss_multiple"] == 1
    assert report["full_case_count"]
    smaller = module.gap(reference["score"] // reference["cases"], 1, suite)
    assert not smaller["full_case_count"]
    assert abs(smaller["ratio_percent"] - 100) < 1e-6
for arguments in [(993823071683, 50, "provisional"), (0, 0, "system"), (0, 50, "unknown")]:
    try:
        module.gap(*arguments)
    except ValueError:
        pass
    else:
        raise AssertionError("mixed suite/invalid count was accepted")
with tempfile.TemporaryDirectory(prefix="ahc001-gap-test-") as directory:
    path = Path(directory) / "scores.csv"
    def csv_report(rows):
        with path.open("w", newline="") as out:
            writer = csv.DictWriter(out, fieldnames=["case_index", "version", "suite", "score", "seed_manifest_md5", "over_5s"])
            writer.writeheader()
            writer.writerows(rows)
        return subprocess.run([sys.executable, str(ROOT / "benchmarks/ahc001_gap_report.py"),
                               "--csv", str(path), "--suite", "system"],
                              capture_output=True, text=True)
    rows = [dict(case_index=i, version="region_sa", suite="system", score=990000000,
                 seed_manifest_md5=module.REFERENCE["system"]["seed_md5"], over_5s=0)
            for i in range(1000)]
    report = json.loads(csv_report(rows).stdout)
    assert report["comparison"].startswith("full published system seeds")
    assert report["local_time_valid"]
    report = json.loads(csv_report(rows[:100]).stdout)
    assert report["comparison"].startswith("REFERENCE ESTIMATE ONLY")
    rows[0]["over_5s"] = 1
    report = json.loads(csv_report(rows).stdout)
    assert not report["local_time_valid"] and "warning" in report
    rows[0]["seed_manifest_md5"] = "wrong"
    report = json.loads(csv_report(rows).stdout)
    assert report["comparison"].startswith("REFERENCE ESTIMATE ONLY")
    assert csv_report([rows[0], rows[0]]).returncode != 0
print("AHC001 reference gap tests passed")
