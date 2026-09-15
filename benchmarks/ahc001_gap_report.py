#!/usr/bin/env python3
"""Report AHC001 gaps without mixing provisional and system-test totals."""
import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = json.loads((ROOT / "practice/ahc001/reference-scores.json").read_text())

def gap(score_sum, cases, suite):
    if suite not in ("provisional", "system") or not 0 < cases <= REFERENCE[suite]["cases"]:
        raise ValueError("invalid suite/case count")
    if not 0 <= score_sum <= cases * 10**9:
        raise ValueError("score exceeds AHC001 maximum; check units and case count")
    reference = REFERENCE[suite]
    mean = score_sum / cases
    reference_mean = reference["score"] / reference["cases"]
    return dict(cases=cases, score_sum=score_sum, mean=mean,
                reference_suite=suite, reference_cases=reference["cases"],
                reference_score=reference["score"], reference_mean=reference_mean,
                mean_gap=reference_mean - mean, ratio_percent=100 * mean / reference_mean,
                loss_multiple=(10**9 - mean) / (10**9 - reference_mean),
                full_case_count=cases == reference["cases"],
                reference_source=reference["source"], reference_verification=REFERENCE["live_recheck"])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--csv", type=Path)
    source.add_argument("--submitted-score", type=int)
    parser.add_argument("--suite", required=True, choices=["provisional", "system"])
    parser.add_argument("--version", default="region_sa")
    args = parser.parse_args()
    if args.submitted_score is not None:
        result = gap(args.submitted_score, REFERENCE[args.suite]["cases"], args.suite)
        result["comparison"] = "submitted total; user must confirm matching test suite"
    else:
        rows = [r for r in csv.DictReader(args.csv.open()) if r["version"] == args.version]
        if not rows: parser.error("no rows for requested version")
        if len({r["case_index"] for r in rows}) != len(rows): parser.error("duplicate cases/repeats: select one run")
        result = gap(sum(int(r["score"]) for r in rows), len(rows), args.suite)
        complete = (args.suite == "system" and len(rows) == 1000 and
                    {int(r["case_index"]) for r in rows} == set(range(1000)) and
                    all(r["suite"] == "system" and r["seed_manifest_md5"] == REFERENCE["system"]["seed_md5"] for r in rows))
        result["comparison"] = ("full published system seeds, local hardware (not official ranking)" if complete
                                 else "REFERENCE ESTIMATE ONLY: incomplete or different inputs")
        result["local_time_valid"] = all(r.get("over_5s") == "0" for r in rows)
        if not result["local_time_valid"]:
            result["warning"] = "timing missing or over 5 seconds; not a time-valid official total"
    print(json.dumps(result, ensure_ascii=False, indent=2))

if __name__ == "__main__": main()
