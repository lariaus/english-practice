#!/usr/bin/env python3
"""Regenerate step2-channel-verdicts.txt (readable format) from
step2-channel-verdicts.csv (source of truth). Two verdicts only: PASS or
FAIL. Borderline calls are PASS with "borderline - ..." noted in the reason.
"""

import csv
from pathlib import Path

HERE = Path(__file__).parent
SRC = HERE / "step2-channel-verdicts.csv"
OUT = HERE / "step2-channel-verdicts.txt"

with open(SRC, newline="", encoding="utf-8") as f:
    rows = list(csv.DictReader(f))

passed = [r for r in rows if r["Verdict"] == "PASS"]
failed = [r for r in rows if r["Verdict"] == "FAIL"]

lines = []
lines.append("STEP 2 RESULTS — Criteria filtering for shadowing channels")
lines.append(f"Total: {len(rows)}  |  Pass: {len(passed)}  |  Fail: {len(failed)}")
lines.append("")
lines.append("=" * 70)
lines.append(f"PASS ({len(passed)})")
lines.append("=" * 70)
lines.append("")
for r in passed:
    lines.append(f"- {r['Channel Title']}")
    lines.append(f"    {r['Channel Link']}")
    lines.append(f"    {r['Reason']}")
    lines.append("")

lines.append("=" * 70)
lines.append(f"FAIL ({len(failed)})")
lines.append("=" * 70)
lines.append("")
for r in failed:
    lines.append(f"- {r['Channel Title']}")
    lines.append(f"    {r['Channel Link']}")
    lines.append(f"    {r['Reason']}")
    lines.append("")

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

print(f"Wrote {OUT} ({len(passed)} pass, {len(failed)} fail)")
