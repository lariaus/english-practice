# NativeExpServer

An experimental, optional companion server for the YT Shadowing tool. It
runs locally, next to (or on the same machine/network as) the web app, and
exposes a small HTTP API for fetching a YouTube video's captions/subtitles -
something the web app can't do on its own from the browser (see the
"subtitles" discussion in the project history: the IFrame Player API doesn't
expose caption text, and YouTube's unofficial caption endpoint has no CORS
headers, so a plain client-side `fetch()` can't read it).

The web app pings `GET /health` first; if this server isn't running or
isn't reachable, the subtitles feature is silently skipped and the rest of
the app works exactly as before. This server is never required.

## Setup (one-time)

```sh
cd native-exp-server
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Run

```sh
source .venv/bin/activate
python3 server.py
```

Listens on `0.0.0.0:5905` by default (reachable from other devices on the
same Wi-Fi, e.g. a phone, same as the app's own dev server) - override with
the `HOST` / `PORT` environment variables if needed:

```sh
PORT=6000 python3 server.py
```

## API

### `GET /health`

Liveness check the web app uses to detect whether this server is running.

```json
{ "status": "ok", "service": "NativeExpServer" }
```

### `GET /subtitles?url=<youtube-url-or-id>&lang=en`

- `url` (required) - a full YouTube URL (`watch`, `youtu.be`, `shorts`,
  `embed`) or a bare 11-character video ID.
- `lang` (optional, default `en`) - BCP-47-ish language code. Tries a
  manually-authored transcript in that language first, then falls back to
  YouTube's auto-generated (ASR) captions in that language.

Success (`200`):

```json
{
  "videoId": "dQw4w9WgXcQ",
  "language": "English",
  "languageCode": "en",
  "isGenerated": false,
  "cues": [
    { "text": "...", "start": 0.5, "duration": 3.2 },
    { "text": "...", "start": 3.7, "duration": 2.9 }
  ]
}
```

`cues` is a flat, chronological list - one entry per caption line, each with
its own start time and on-screen duration in seconds.

Failure (`400`/`404`/`502`) - JSON body with an `error` message, and for a
missing-language case, an `availableLanguages` list of what YouTube does
have for that video:

```json
{ "error": "No 'fr' captions (manual or auto-generated) found for this video.", "availableLanguages": ["en"] }
```

## Notes

- No authentication - this is a local/LAN-only experimental tool, not meant
  to be exposed to the public internet.
- Relies on [`youtube-transcript-api`](https://github.com/jdepoix/youtube-transcript-api),
  an unofficial library scraping YouTube's own caption data - it can break
  if YouTube changes something, and is rate-limited/blocked the same way any
  scraper would be under heavy use.
