# native-server

C++ replacement for the ad hoc `python3 -m http.server 8000 --directory
dist` command (see `../docs/migration-to-offline-app/step-2.md` for the
full design). Serves a directory of static files - built as an embeddable
library (`native_server_core`) with a thin CLI wrapper (`native_server_cli`)
on top, so it can later be linked directly into a native app instead of run
as a subprocess (iOS doesn't allow spawning arbitrary subprocesses).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Requires network access at configure time (CMake `FetchContent` pulls
cpp-httplib, nlohmann/json, and Catch2, each pinned to a specific tag) -
build-time only, unrelated to this project's offline-first runtime goals.

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

or run the test binary directly: `./build/tests/native_server_tests`.

## Scope

- Static file serving only, for now. No `/health` or `/subtitles` endpoints
  yet - those arrive when `native-exp-server`'s job gets folded into this
  same server in a later migration step.
- No HTTPS/TLS - `localhost` already counts as a secure context for browser
  APIs like `getUserMedia`, so this server never needs to terminate TLS
  itself; the existing Cloudflare-tunnel-for-remote-HTTPS approach is
  unaffected.
- No response compression.

See `../docs/migration-to-offline-app/step-2.md` for the full design
rationale, and `CODING_STYLE.md` for this subproject's naming/structure
conventions.
