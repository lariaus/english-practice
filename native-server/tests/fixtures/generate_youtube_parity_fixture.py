#!/usr/bin/env python3
"""Captures real Python youtube_transcript_api output for one video/
language as JSON - the reference data test_youtube_utils_parity.cpp
compares the live C++ path against.

Invoked automatically by that test (see REGENERATE_PYTHON_REF_DATA in
docs/migration-to-offline-app/step-4.md) - not meant to be run by hand
normally. Requires youtube_transcript_api to be installed (`pip install
youtube-transcript-api` into any active venv).

Usage: generate_youtube_parity_fixture.py <video_id> <lang> <output_path>
"""
import json
import sys

from youtube_transcript_api import YouTubeTranscriptApi


def main():
    video_id, lang, output_path = sys.argv[1], sys.argv[2], sys.argv[3]

    api = YouTubeTranscriptApi()
    transcript_list = api.list(video_id)
    transcript = transcript_list.find_transcript([lang])
    fetched = transcript.fetch()

    data = {
        "videoId": video_id,
        "language": transcript.language,
        "languageCode": transcript.language_code,
        "isGenerated": transcript.is_generated,
        "cues": [{"text": s.text, "start": s.start, "duration": s.duration} for s in fetched],
    }

    with open(output_path, "w") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


if __name__ == "__main__":
    main()
