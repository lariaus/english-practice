#!/usr/bin/env python3
"""Look up US IPA phonemic transcriptions on the Cambridge Dictionary
for a list of words and write them out as a CSV.

Usage:
    python3 scripts/cambridge_us_phonemes.py input.txt output.csv

Input file: one word per line. Each line is trimmed and lowercased.
Output CSV:
    Front,Back
    word, /transcript/
    word, /transcript/ || /transcript/
"""

import html
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE_URL = "https://dictionary.cambridge.org/us/dictionary/english/"
USER_AGENT = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
)
REQUEST_DELAY_SECONDS = 3

# Anchors: a US pronunciation block, then the IPA span that follows it.
US_PRON_RE = re.compile(r'<span class="us dpron-i[^"]*">')
IPA_START_RE = re.compile(r'<span class="ipa dipa[^"]*">')
SPAN_TAG_RE = re.compile(r'<(/?)span\b[^>]*>')
TAG_RE = re.compile(r'<[^>]+>')

# The IPA span can contain nested <span> tags (e.g. Cambridge wraps some
# schwas in <span class="sp dsp">), so a naive non-greedy match up to the
# first </span> truncates the transcript. Track span depth instead so we
# stop at the IPA span's *own* closing tag.
def extract_span_text(body, content_start):
    depth = 1
    for tag in SPAN_TAG_RE.finditer(body, content_start):
        depth += -1 if tag.group(1) else 1
        if depth == 0:
            inner_html = body[content_start:tag.start()]
            return html.unescape(TAG_RE.sub("", inner_html)).strip()
    return None


def fetch_us_transcripts(word):
    url = BASE_URL + urllib.parse.quote(word)
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            body = response.read().decode("utf-8")
            redirected = urllib.parse.urlparse(response.geturl()).path != urllib.parse.urlparse(url).path
    except urllib.error.HTTPError as error:
        print(f"warning: {word!r} -> HTTP {error.code}", file=sys.stderr)
        return [], False

    transcripts = []
    for us_match in US_PRON_RE.finditer(body):
        ipa_match = IPA_START_RE.search(body, us_match.end(), us_match.end() + 1000)
        if not ipa_match:
            continue
        ipa = extract_span_text(body, ipa_match.end())
        if ipa and ipa not in transcripts:
            transcripts.append(ipa)
    if not transcripts:
        print(f"warning: no US transcript found for {word!r}", file=sys.stderr)
    return transcripts, redirected


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <input.txt> <output.csv>", file=sys.stderr)
        return 1

    input_path, output_path = sys.argv[1], sys.argv[2]

    with open(input_path, encoding="utf-8") as input_file:
        words = [line.strip().lower() for line in input_file if line.strip()]

    rows = []
    for index, word in enumerate(words):
        if index > 0:
            time.sleep(REQUEST_DELAY_SECONDS)
        print(f"[{index + 1}/{len(words)}] {word}", file=sys.stderr)
        transcripts, redirected = fetch_us_transcripts(word)
        if not transcripts:
            back = "[[warn: not found]]"
        else:
            back = " || ".join(f"/{t}/" for t in transcripts)
            if redirected:
                back += " [[warn: redirect]]"
        rows.append(f"{word}, {back}")

    with open(output_path, "w", encoding="utf-8") as output_file:
        output_file.write("Front,Back\n")
        output_file.write("\n".join(rows) + "\n")

    return 0


if __name__ == "__main__":
    sys.exit(main())
