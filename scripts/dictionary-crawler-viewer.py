#!/usr/bin/env python3
"""GUI viewer for dictionary-crawler results.

Usage:
  /path/to/.venv/bin/python3 ./scripts/dictionary-crawler-viewer.py <path-to-word-list.txt>
  /path/to/.venv/bin/python3 ./scripts/dictionary-crawler-viewer.py <path-to-word-list.txt> --json

Reads the same word-list file dictionary-crawler itself reads (comments,
@output_dir/@api/@interval_time parameters, then one word per line), plus
the status file it produces (<name>-status.json, next to the word list) and
each source's own on-disk cache under @output_dir, and builds one JSON
document describing every word:

    {"title": ..., "summary": {...}, "words": [{"word": ..., "found": ...,
     "def_source": ..., "ipa": {"text", "source", "accent"} | null,
     "audio": {"path", "accent"} | null}, ...]}

With --json, that document is printed to stdout and the program exits - no
server, no GUI. Without it, the same document is rendered as an HTML table:

    Word | Found | Defs | US IPA | US Audio

US IPA shows the actual IPA text if a US-recognized pronunciation was found
(preferring WiktionaryAPIEntry's own cache, falling back to
FreeDictionaryAPIEntry's), or a red "not found" if not. US Audio shows a
Play button if WiktionaryAPIEntry downloaded a real US recording, or a red
"not found" if not - FreeDictionaryAPIEntry never has audio at all.

This is a read-only companion to the crawler: it never writes anything, and
re-reads the status file + caches fresh on every page load (or --json run),
so it's safe to run alongside a crawl still in progress - reload the page
to see updated results.

Architecture: a tiny local HTTP server (stdlib only) rebuilds the JSON and
renders it as HTML fresh on every request, and serves each source's audio
files. Prints its URL for you to open yourself - use Chrome specifically if
you want Play to actually produce sound: it decodes Ogg Vorbis natively,
unlike macOS's embedded WKWebView (Safari's engine), which - confirmed by
hand - fails every Wiktionary recording (*.ogg) with a "source not
supported" MediaError. The server keeps running until you stop it with
Ctrl+C.
"""

import argparse
import html
import http.server
import json
import socketserver
import sys
from pathlib import Path
from urllib.parse import quote, unquote, urlparse

# Fixed rather than OS-assigned, so the URL stays the same across restarts -
# you can just hit reload in the browser instead of re-navigating each time.
PORT = 8765

# Mirrors dictionary_utils::DictionaryEntry::normalizeWord()'s hardcoded
# exception set - words whose only real English entry is capitalized (see
# native-server/dictionary_utils/lib/dictionary_entry.cpp).
CAPITALIZED_WORDS = {"september"}

# Mirrors isUsAccentTag()'s exact-match set
# (native-server/dictionary_utils/lib/wiktionary_api_entry.cpp).
US_EXACT_TAGS = {"us", "ga", "genam"}

# Mirrors usStateNames() in the same file - a curated, non-exhaustive list;
# see that function's own comment for why "Georgia" is an accepted ambiguity.
US_STATES = {
    "alabama", "alaska", "arizona", "arkansas", "california", "colorado",
    "connecticut", "delaware", "florida", "georgia", "hawaii", "idaho",
    "illinois", "indiana", "iowa", "kansas", "kentucky", "louisiana",
    "maine", "maryland", "massachusetts", "michigan", "minnesota",
    "mississippi", "missouri", "montana", "nebraska", "nevada",
    "new hampshire", "new jersey", "new mexico", "new york",
    "north carolina", "north dakota", "ohio", "oklahoma", "oregon",
    "pennsylvania", "rhode island", "south carolina", "south dakota",
    "tennessee", "texas", "utah", "vermont", "virginia", "washington",
    "west virginia", "wisconsin", "wyoming",
}

US_COMPASS_PREFIXES = ("northern ", "southern ", "eastern ", "western ", "upper ", "lower ", "central ")


def normalize_word(word):
    trimmed = word.strip()
    lower = trimmed.lower()
    if lower in CAPITALIZED_WORDS:
        return lower[0].upper() + lower[1:]
    return lower


def percent_encode(value):
    # Mirrors free_dictionary_api_entry.cpp/wiktionary_api_entry.cpp's own
    # percent-encoders: keep ASCII alnum/-_.~, escape every other byte.
    out = []
    for byte in value.encode("utf-8"):
        ch = chr(byte)
        if byte < 128 and (ch.isalnum() or ch in "-_.~"):
            out.append(ch)
        else:
            out.append("%%%02X" % byte)
    return "".join(out)


def is_us_accent_tag(tag):
    lower = tag.lower()
    if lower in US_EXACT_TAGS or "general american" in lower:
        return True
    if "us" in lower.split():
        return True
    if lower in US_STATES:
        return True
    for prefix in US_COMPASS_PREFIXES:
        if lower.startswith(prefix) and lower[len(prefix):] in US_STATES:
            return True
    return False


def has_us_accent(tags):
    return any(is_us_accent_tag(t) for t in tags)


def parse_word_list(path):
    output_dir = None
    api_names = []
    words = []

    for raw_line in path.read_text().splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("@"):
            tokens = line[1:].split()
            if not tokens:
                continue
            name, values = tokens[0], tokens[1:]
            if name == "output_dir" and values:
                output_dir = values[0]
            elif name == "api" and values:
                api_names = values
            continue
        words.append(line)

    if not api_names:
        api_names = ["freedictionaryapi", "wikidictionary"]
    resolved_output_dir = (path.parent / output_dir) if output_dir else path.parent
    return resolved_output_dir.resolve(), api_names, words


def load_wiktionary_cache(output_dir, word):
    path = output_dir / "dictionaries" / "wiktionaryapi" / f"en-{percent_encode(word)}.json"
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text())
    except json.JSONDecodeError:
        return None


def load_freedictionary_cache(output_dir, word):
    path = output_dir / "dictionaries" / "freedictionaryapi" / f"en-{percent_encode(word)}.json"
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text())
    except json.JSONDecodeError:
        return None


def wiktionary_us_ipa(cache):
    """Mirrors buildPhonetics()'s own merge rule
    (native-server/dictionary_utils/lib/dictionary_entry.cpp): a lone
    untagged IPA line counts as the US one too when there's a confirmed US
    audio recording, since Wiktionary's own convention is that an untagged
    line applies regardless of dialect. Returns (text, raw_accent_tags) -
    raw_accent_tags is [] for the untagged-merge case (nothing to show but
    the inference itself), or None (alongside None text) if not found at
    all - this is the actual accent info dictionary_utils itself throws
    away once it collapses these into just "US" (see PhoneticEntry's own
    doc comment)."""
    ipa_entries = cache.get("ipa", [])

    for entry in ipa_entries:
        accents = entry.get("accents", [])
        if entry.get("text") and has_us_accent(accents):
            return entry["text"], accents

    has_us_audio = any(a.get("audio") for a in cache.get("audio", []))
    untagged = [e for e in ipa_entries if e.get("text") and not e.get("accents")]
    if has_us_audio and len(untagged) == 1:
        return untagged[0]["text"], []

    return None, None


def wiktionary_us_audio_path(cache):
    """Returns (relative_path, raw_accent_tags) - relative_path is the same
    ServerData-relative string the C++ cache already stores (e.g.
    "dictionaries/wiktionaryapi/en-us-word.ogg"), not resolved against
    @output_dir - that resolution happens only where a file actually needs
    to be read (the /audio/ route) or serialized (nowhere else)."""
    for entry in cache.get("audio", []):
        if entry.get("audio"):
            return entry["audio"], entry.get("accents", [])
    return None, None


def freedictionary_us_ipa(cache):
    for entry in cache.get("entries", []):
        for pron in entry.get("pronunciations", []):
            tags = pron.get("tags", [])
            if pron.get("text") and has_us_accent(tags):
                return pron["text"], tags
    return None, None


def freedictionary_has_definitions(cache):
    return any(sense.get("definition") for entry in cache.get("entries", []) for sense in entry.get("senses", []))


def build_word_entry(raw_word, output_dir):
    word = normalize_word(raw_word)

    wikt_cache = load_wiktionary_cache(output_dir, word)
    free_cache = load_freedictionary_cache(output_dir, word)
    found = wikt_cache is not None or free_cache is not None

    ipa_text, ipa_accent, audio_path, audio_accent = None, None, None, None
    if wikt_cache:
        ipa_text, ipa_accent = wiktionary_us_ipa(wikt_cache)
        audio_path, audio_accent = wiktionary_us_audio_path(wikt_cache)
    ipa_source = "WDic" if ipa_text is not None else None

    if ipa_text is None and free_cache:
        ipa_text, ipa_accent = freedictionary_us_ipa(free_cache)
        if ipa_text is not None:
            ipa_source = "FreeDic"

    def_source = "FreeDic" if free_cache and freedictionary_has_definitions(free_cache) else None

    return {
        "word": word,
        "found": found,
        "def_source": def_source,
        "ipa": {"text": ipa_text, "source": ipa_source, "accent": ipa_accent} if ipa_text is not None else None,
        "audio": {"path": audio_path, "accent": audio_accent} if audio_path is not None else None,
    }


def build_data(word_list_path, title):
    output_dir, _api_names, raw_words = parse_word_list(word_list_path)
    words = [build_word_entry(raw_word, output_dir) for raw_word in raw_words]

    total = len(words)
    ok = sum(1 for w in words if w["ipa"] and w["audio"])
    not_found = sum(1 for w in words if not w["found"])
    missing_ipa = sum(1 for w in words if not w["ipa"])
    missing_audio = sum(1 for w in words if not w["audio"])

    return {
        "title": title,
        "output_dir": str(output_dir),
        "summary": {
            "total": total,
            "ok": ok,
            "not_found": not_found,
            "missing_ipa": missing_ipa,
            "missing_audio": missing_audio,
        },
        "words": words,
    }


PAGE_TEMPLATE = """<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
  body {{ font-family: -apple-system, Helvetica, Arial, sans-serif; margin: 20px; color: #1a1a1a; }}
  .summary {{ font-size: 15px; font-weight: 600; margin-bottom: 16px; }}
  table {{ border-collapse: collapse; width: 100%; }}
  th, td {{ text-align: left; padding: 8px 14px; border-bottom: 1px solid #ddd; }}
  th {{ background: #f5f5f5; font-size: 13px; text-transform: uppercase; letter-spacing: 0.03em; }}
  .missing {{ color: #c62828; }}
  .found-col {{ text-align: center; font-size: 12px; }}
  .found-yes {{ color: #2e7d32; }}
  .found-no {{ color: #c62828; }}
  /* Matches src/components/DictionaryPopup.vue's .dictionary-phonetic-text:
     var(--text-dim) (src/style.css) at 0.95rem, plain system sans-serif. */
  .found {{ color: #9198a3; font-size: 0.95rem; }}
  .source-tag {{ color: #000; font-size: 14px; font-weight: 700; }}
  .accent-tag {{ color: #666; font-size: 12px; font-style: italic; }}
  .play-btn {{ cursor: pointer; padding: 4px 12px; }}
  .play-error {{ color: #c62828; margin-left: 8px; font-size: 12px; }}
  .incomplete-row {{ background: #fdecea; }}
</style>
<script>
  // MediaError.code values - https://developer.mozilla.org/docs/Web/API/MediaError/code
  var MEDIA_ERROR_MESSAGES = {{
    1: "aborted",
    2: "network error",
    3: "decode error - format not supported by this player",
    4: "source not supported - format not supported by this player"
  }};

  function playAudio(button, url) {{
    var errorSpan = button.nextElementSibling;
    errorSpan.textContent = "";

    var audio = new Audio(url);
    audio.addEventListener("error", function () {{
      var code = audio.error ? audio.error.code : null;
      var reason = MEDIA_ERROR_MESSAGES[code] || "unknown error";
      errorSpan.textContent = "✗ " + reason;
      console.error("Audio error for", url, audio.error);
    }});

    var playPromise = audio.play();
    if (playPromise && typeof playPromise.catch === "function") {{
      playPromise.catch(function (err) {{
        errorSpan.textContent = "✗ " + err.name + ": " + err.message;
      }});
    }}
  }}
</script>
</head>
<body>
  <div class="summary">words: {total} &nbsp; ok: {ok} &nbsp; not found: {not_found} &nbsp; missing IPA: {missing_ipa} &nbsp; missing Audio: {missing_audio}</div>
  <table>
    <tr><th>Word</th><th>Found</th><th>Defs</th><th>US IPA</th><th>US Audio</th></tr>
{rows}
  </table>
</body>
</html>
"""


def render_page(data):
    row_lines = []
    for w in data["words"]:
        ipa = w["ipa"]
        if ipa:
            accent_label = ", ".join(ipa["accent"]) if ipa["accent"] else "Auto"
            ipa_cell = (
                f'<span class="source-tag">[{ipa["source"]}]</span> '
                f'<span class="accent-tag">[{html.escape(accent_label)}]</span> '
                f'<span class="found">{html.escape(ipa["text"])}</span>'
            )
        else:
            ipa_cell = '<span class="missing">&#10007;</span>'

        audio = w["audio"]
        if audio:
            audio_url = "/audio/" + quote(audio["path"])
            accent_label = ", ".join(audio["accent"]) if audio["accent"] else "en-us- filename"
            audio_cell = (
                '<span class="source-tag">[WDic]</span> '
                f'<span class="accent-tag">[{html.escape(accent_label)}]</span> '
                f"<button class=\"play-btn\" onclick=\"playAudio(this, '{audio_url}')\">"
                '&#9654; Play</button><span class="play-error"></span>'
            )
        else:
            audio_cell = '<span class="missing">&#10007;</span>'

        found_cell = (
            '<span class="found-yes">&#10003;</span>' if w["found"] else '<span class="found-no">&#10007;</span>'
        )
        def_cell = (
            f'<span class="source-tag">[{w["def_source"]}]</span>'
            if w["def_source"]
            else '<span class="found-no">&#10007;</span>'
        )

        row_class = "" if (ipa and audio) else ' class="incomplete-row"'
        row_lines.append(
            f"    <tr{row_class}><td>{html.escape(w['word'])}</td>"
            f'<td class="found-col">{found_cell}</td>'
            f'<td class="found-col">{def_cell}</td>'
            f"<td>{ipa_cell}</td><td>{audio_cell}</td></tr>"
        )

    return PAGE_TEMPLATE.format(
        title=html.escape(data["title"]),
        rows="\n".join(row_lines),
        **data["summary"],
    )


def make_handler(word_list_path, output_dir):
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, format_, *args):
            pass  # keep the terminal quiet

        def do_GET(self):
            parsed = urlparse(self.path)

            if parsed.path in ("/", "/index.html"):
                data = build_data(word_list_path, word_list_path.name)
                body = render_page(data).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return

            if parsed.path.startswith("/audio/"):
                relative = unquote(parsed.path[len("/audio/"):])
                file_path = (output_dir / relative).resolve()
                if output_dir not in file_path.parents or not file_path.exists():
                    self.send_error(404)
                    return
                suffix = file_path.suffix.lower()
                content_type = "audio/ogg" if suffix in (".ogg", ".oga") else "audio/wav"
                data = file_path.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return

            self.send_error(404)

    return Handler


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("word_list_path")
    parser.add_argument(
        "--json", action="store_true", help="dump the data as JSON to stdout and exit - no server, no GUI"
    )
    args = parser.parse_args()

    word_list_path = Path(args.word_list_path).expanduser().resolve()
    if not word_list_path.exists():
        print(f"dictionary-crawler-viewer: no such file: {word_list_path}", file=sys.stderr)
        return 1

    if args.json:
        data = build_data(word_list_path, word_list_path.name)
        print(json.dumps(data, ensure_ascii=False, indent=2))
        return 0

    output_dir, _api_names, _words = parse_word_list(word_list_path)

    handler = make_handler(word_list_path, output_dir)
    socketserver.TCPServer.allow_reuse_address = True  # rebind the fixed port immediately on restart
    try:
        httpd = socketserver.TCPServer(("127.0.0.1", PORT), handler)
    except OSError as e:
        print(
            f"dictionary-crawler-viewer: could not bind to port {PORT} ({e}) - "
            "is another instance already running? Stop it first.",
            file=sys.stderr,
        )
        return 1
    url = f"http://127.0.0.1:{PORT}/"

    print(f"URL: {url}")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.shutdown()

    return 0


if __name__ == "__main__":
    sys.exit(main())
