#!/usr/bin/env python3
"""NativeExpServer: local companion server for the English Practice app's
YT Shadowing tool.

Optional, opt-in companion process. The web app checks whether this server
is reachable (GET /health) and, only if so, uses it to fetch YouTube caption
data - otherwise that functionality is silently skipped. The app works fine
without this server running at all.

Run:
    python3 server.py
See README.md for one-time setup (venv + pip install).
"""

import json
import os
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

from youtube_transcript_api import (
    YouTubeTranscriptApi,
    NoTranscriptFound,
    TranscriptsDisabled,
    VideoUnavailable,
)

HOST = os.environ.get("HOST", "0.0.0.0")
PORT = int(os.environ.get("PORT", "5905"))
DEFAULT_LANGUAGE = "en"

VIDEO_ID_RE = re.compile(r"^[\w-]{11}$")


def parse_youtube_video_id(raw):
    """Mirrors src/engine/ytShadowingEngine.js's parseYouTubeVideoId - accepts
    a full YouTube URL (watch/shorts/youtu.be/embed) or a bare 11-char video
    ID, returns the video ID or None if it can't be parsed."""
    if not raw:
        return None
    trimmed = raw.strip()
    if VIDEO_ID_RE.match(trimmed):
        return trimmed

    parsed = urlparse(trimmed)
    if not parsed.scheme or not parsed.netloc:
        return None

    hostname = parsed.hostname or ""

    if hostname == "youtu.be":
        candidate = parsed.path.lstrip("/")
        return candidate if VIDEO_ID_RE.match(candidate) else None

    if "youtube.com" in hostname:
        query = parse_qs(parsed.query)
        v_param = (query.get("v") or [None])[0]
        if v_param and VIDEO_ID_RE.match(v_param):
            return v_param

        match = re.search(r"/(?:embed|shorts)/([\w-]{11})", parsed.path)
        if match:
            return match.group(1)

    return None


class Handler(BaseHTTPRequestHandler):
    def _send_json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/health":
            self._send_json(200, {"status": "ok", "service": "NativeExpServer"})
            return
        if parsed.path == "/subtitles":
            self._handle_subtitles(parse_qs(parsed.query))
            return
        self._send_json(404, {"error": "Not found"})

    def _handle_subtitles(self, query):
        raw_url = (query.get("url") or [None])[0]
        language = (query.get("lang") or [DEFAULT_LANGUAGE])[0]

        video_id = parse_youtube_video_id(raw_url)
        if not video_id:
            self._send_json(
                400,
                {"error": "Missing or unparseable 'url' query parameter (expected a YouTube URL or video ID)."},
            )
            return

        api = YouTubeTranscriptApi()
        try:
            transcript_list = api.list(video_id)
        except VideoUnavailable:
            self._send_json(404, {"error": "Video unavailable or does not exist."})
            return
        except TranscriptsDisabled:
            self._send_json(404, {"error": "Captions are disabled for this video."})
            return
        except Exception as exc:
            self._send_json(502, {"error": f"Could not look up captions: {exc}"})
            return

        # find_transcript() already prefers a manually-created transcript in
        # this language over an auto-generated one, falling back to
        # auto-generated only if no manual track exists - no separate
        # generated-only lookup needed on top of it.
        transcript = None
        try:
            transcript = transcript_list.find_transcript([language])
        except NoTranscriptFound:
            pass

        if transcript is None:
            available = sorted({t.language_code for t in transcript_list})
            self._send_json(
                404,
                {
                    "error": f"No '{language}' captions (manual or auto-generated) found for this video.",
                    "availableLanguages": available,
                },
            )
            return

        try:
            fetched = transcript.fetch()
        except Exception as exc:
            self._send_json(502, {"error": f"Could not fetch captions: {exc}"})
            return

        self._send_json(
            200,
            {
                "videoId": video_id,
                "language": transcript.language,
                "languageCode": transcript.language_code,
                "isGenerated": transcript.is_generated,
                "cues": [
                    {"text": s.text, "start": s.start, "duration": s.duration}
                    for s in fetched
                ],
            },
        )

    def log_message(self, format, *args):
        print(f"{self.address_string()} - {format % args}")


def main():
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"NativeExpServer listening on http://{HOST}:{PORT}")
    print("Endpoints: GET /health, GET /subtitles?url=<youtube-url-or-id>&lang=en")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
