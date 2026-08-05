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
youtube-transcript-api` into any venv) the first time it runs for a given
video, or whenever `REGENERATE_PYTHON_REF_DATA` is set - otherwise it
reuses its cached reference data (`build/youtube_parity_cache/`,
gitignored) with no Python dependency.

## Scope

- Static file serving, plus `GET /health` and `GET /subtitles` (YouTube
  caption fetching) and `GET`/`PUT`/`DELETE /storage/maps/:mapId/:key`
  (small local key-value storage).
- No HTTPS/TLS *server* - `localhost` already counts as a secure context
  for browser APIs like `getUserMedia`, so this server never needs to
  terminate TLS itself; the existing Cloudflare-tunnel-for-remote-HTTPS
  approach is unaffected. (It does make outbound HTTPS *client* requests,
  via `https_client`, to fetch captions from YouTube.)
- No response compression.

See `CODING_STYLE.md` for this subproject's naming/structure conventions.
