#!/usr/bin/env python3
"""Check the two objective shadowing criteria for each channel via yt-dlp:

- criterion: "must have preferably many videos" (< 10 videos = fail)
- criterion: "avoid very short videos" (90%+ under 3 min = fail)

Samples up to SAMPLE_SIZE most recent videos per channel (fast, no download).
Writes video-stats.csv with the raw numbers so step 2's judgment agents don't
have to guess at video count / length.
"""

import csv
import json
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).parent
CSV_PATH = HERE / "unique-youtube-channels.csv"
OUT_PATH = HERE / "video-stats.csv"
YTDLP = str(HERE / ".venv" / "bin" / "python")
SAMPLE_SIZE = 20
SHORT_THRESHOLD_SEC = 180


def channel_videos_url(url):
    url = url.rstrip("/")
    return url if url.endswith("/videos") else url + "/videos"


def fetch_durations(url):
    cmd = [
        YTDLP, "-m", "yt_dlp",
        "--flat-playlist",
        "--print", "%(duration)s",
        "--playlist-end", str(SAMPLE_SIZE),
        "--no-warnings",
        channel_videos_url(url),
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return None, "timeout"
    if result.returncode != 0 and not result.stdout.strip():
        return None, (result.stderr.strip().splitlines()[-1] if result.stderr.strip() else "error")
    durations = []
    for line in result.stdout.strip().splitlines():
        line = line.strip()
        if line and line != "NA":
            try:
                durations.append(float(line))
            except ValueError:
                pass
    return durations, None


def analyze(row):
    title, link = row["Channel Title"], row["Channel Link"]
    durations, err = fetch_durations(link)
    if err:
        return {**row, "sample_count": "", "has_min_10_videos": "", "pct_under_3min": "", "error": err}
    count = len(durations)
    has_min_10 = count >= 10
    pct_short = (sum(1 for d in durations if d < SHORT_THRESHOLD_SEC) / count) if count else 0
    return {
        **row,
        "sample_count": count,
        "has_min_10_videos": has_min_10,
        "pct_under_3min": round(pct_short * 100, 1),
        "error": "",
    }


def main():
    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    results = []
    with ThreadPoolExecutor(max_workers=6) as pool:
        future_to_row = {pool.submit(analyze, row): row for row in rows}
        for i, future in enumerate(as_completed(future_to_row), 1):
            row = future_to_row[future]
            res = future.result()
            print(f"[{i}/{len(rows)}] {row['Channel Title']}: "
                  f"count={res['sample_count']} min10={res['has_min_10_videos']} "
                  f"pct_short={res['pct_under_3min']} err={res['error']}")
            results.append(res)

    results.sort(key=lambda r: rows.index({"Channel Title": r["Channel Title"], "Channel Link": r["Channel Link"]}))

    fieldnames = ["Channel Title", "Channel Link", "sample_count", "has_min_10_videos", "pct_under_3min", "error"]
    with open(OUT_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)

    print(f"\nWrote {OUT_PATH.name}")


if __name__ == "__main__":
    main()
