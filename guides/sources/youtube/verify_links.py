#!/usr/bin/env python3
"""Verify each YouTube channel link in unique-youtube-channels.csv is reachable.

Follows redirects and checks the final HTTP status code: 200 = valid,
404 (or any non-200) = broken. Rewrites the CSV to keep only valid rows
and writes removed rows to removed-broken-links.csv.
"""

import csv
import sys
import urllib.request
import urllib.error
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).parent
CSV_PATH = HERE / "unique-youtube-channels.csv"
REMOVED_PATH = HERE / "removed-broken-links.csv"

USER_AGENT = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)


def check_url(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return resp.status == 200
    except urllib.error.HTTPError as e:
        return e.code == 200
    except Exception as e:
        print(f"  error checking {url}: {e}", file=sys.stderr)
        return False


def main():
    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    valid_rows = []
    removed_rows = []

    with ThreadPoolExecutor(max_workers=8) as pool:
        future_to_row = {pool.submit(check_url, row["Channel Link"]): row for row in rows}
        for i, future in enumerate(as_completed(future_to_row), 1):
            row = future_to_row[future]
            ok = future.result()
            status = "OK" if ok else "BROKEN"
            print(f"[{i}/{len(rows)}] {status}: {row['Channel Title']} -> {row['Channel Link']}")
            (valid_rows if ok else removed_rows).append(row)

    valid_rows.sort(key=lambda r: rows.index(r))
    removed_rows.sort(key=lambda r: rows.index(r))

    with open(CSV_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Channel Title", "Channel Link"])
        writer.writeheader()
        writer.writerows(valid_rows)

    with open(REMOVED_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Channel Title", "Channel Link"])
        writer.writeheader()
        writer.writerows(removed_rows)

    print(f"\n{len(valid_rows)} valid, {len(removed_rows)} broken (removed).")
    if removed_rows:
        print(f"Removed rows written to {REMOVED_PATH.name}")


if __name__ == "__main__":
    main()
