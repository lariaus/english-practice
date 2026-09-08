# native-server

C++ replacement for the ad hoc `python3 -m http.server 8000 --directory
dist` command. Serves a directory of static files - built as an embeddable
library (`native_server_core`) with a thin CLI wrapper (`native_server_cli`)
on top, so it can later be linked directly into a native app instead of run
as a subprocess (iOS doesn't allow spawning arbitrary subprocesses).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Requires network access at configure time (CMake `FetchContent` pulls
cpp-httplib, nlohmann/json, pugixml, and Catch2, each pinned to a specific
tag) - build-time only, unrelated to this project's offline-first runtime
goals.

Apple-only (macOS/iOS) from here on - `https_client` wraps `NSURLSession`
(Objective-C++), not OpenSSL, so there's no Linux build.

## Run

```sh
./build/native_server_cli/native_server_cli --dir /path/to/dist [--host 0.0.0.0] [--port 8000]
```

Equivalent env vars: `NATIVE_SERVER_DIR`, `HOST`, `PORT` (a CLI flag takes
precedence over its env var). `--dir`/`NATIVE_SERVER_DIR` is required -
there's no default directory.

## Test

```sh
ctest --test-dir build --output-on-failure
```

One binary per test file (see `tests/CMakeLists.txt`'s header comment) -
`ctest` output is one line per file. Run an individual binary directly
(e.g. `./build/tests/native_server_test_subtitles_route`) for full Catch2
detail, optionally with `--success` or a `[tag]` filter.

Most tests need live network access (they hit real HTTPS servers,
including real YouTube) - deliberate, matching Python's actual live
behavior is the goal. The one parity test
(`native_server_test_youtube_utils_parity`) additionally needs a Python
environment with `youtube_transcript_api` installed (`pip install
youtube-transcript-api` into any venv, or `pip3 install
--break-system-packages youtube-transcript-api` for a plain Homebrew
`python3` with no venv active) the first time it runs for a given video,
or whenever `REGENERATE_PYTHON_REF_DATA` is set - otherwise it reuses its
cached reference data (`build/youtube_parity_cache/`, gitignored) with no
Python dependency. If it starts failing on real cue text (not a network
error), the live YouTube captions likely just changed since the fixture
was captured - rerun with `REGENERATE_PYTHON_REF_DATA=1` to refresh it.

The dictionary live tests (`*_dictionary_*_live`, `*_wiktionary_*_live`)
hit real Wiktionary/freedictionaryapi.com traffic and can occasionally hit
Wiktionary's own rate limit (HTTP 429) when several of these run back to
back - see `tests/support/live_retry.h`: a confirmed 429 (checked twice,
2s apart) is treated as a genuine pass, not a failure; anything else fails
normally. `DICTIONARY_IGNORE_CACHE` (set to any non-empty value on the
`native_server_cli` process) bypasses every dictionary cache layer for
manual/live testing.

## Scope

- Static file serving, plus `GET /health`, `GET /subtitles` (YouTube
  caption fetching), `GET /dictionary` (word lookup - definitions from
  freedictionaryapi.com, real pronunciation audio + IPA from Wiktionary's
  own API, merged and cached to disk under `server_data/dictionaries/` -
  see `docs/dictionary-spec.md`), and `GET`/`PUT`/`DELETE
  /storage/maps/:mapId/:key` (small local key-value storage).
- No HTTPS/TLS *server* - `localhost` already counts as a secure context
  for browser APIs like `getUserMedia`, so this server never needs to
  terminate TLS itself; the existing Cloudflare-tunnel-for-remote-HTTPS
  approach is unaffected. (It does make outbound HTTPS *client* requests,
  via `https_client`, to fetch captions from YouTube.)
- No response compression.

See `CODING_STYLE.md` for this subproject's naming/structure conventions.
