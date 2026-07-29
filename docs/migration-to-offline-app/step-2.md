# Step 2: Rewrite the static Python server as a C++ static server (`native-server/`)

Implementation plan for checklist step 2 (see `A-vision-and-goals.md`,
`C-migration-steps.md`). Written up during planning, before any code
existed - kept here as the record of *why* things are shaped the way they
are, not just a stale TODO list once the code exists too.

## Context

Today, "the static server" isn't real code at all - it's the README-
documented command `python3 -m http.server 8000 --directory dist`. This
step replaces it with a real, tested C++ project (`native-server/`), while
explicitly *not* yet touching `native-exp-server/` (the Python companion
server that fetches YouTube captions) - that merge is step 3.

A decision made while planning this step drives almost every other choice
below: **iOS cannot spawn arbitrary subprocesses** the way macOS/Linux can.
The eventual native Xcode app will need to run this server *in-process*, on
a background thread inside the app's own binary - not as a spawned child
executable. So this server has to be built as an embeddable library from
day one, with a thin CLI wrapper on top for today's local dev/testing use,
rather than a single monolithic `main()`-owns-everything program that would
need restructuring later.

## Decisions from planning

- **HTTP library**: cpp-httplib (single header, no heavy deps, has built-in
  static-file mounting + a thread pool).
- **Build system**: CMake.
- **Test framework**: Catch2, with both unit tests (pure helpers) and
  integration tests (boot the real server, hit it with real HTTP requests).
- **JSON library**: nlohmann/json, vendored now (not deferred) since step 3
  will need real JSON payloads (caption cue arrays) very soon.
- **Dependency management**: CMake `FetchContent` for all three
  (cpp-httplib, nlohmann/json, Catch2), pinned to specific released tags.
  Fine to require network at *configure* time - this project's
  offline-first goal is about the running app, not the build process.
- **Location/name**: new top-level `native-server/` directory - forward-
  looking name since step 3 folds `native-exp-server`'s job into this same
  binary.
- **Structure**: a core library target (embeddable, no `main()`, no process-
  wide side effects) + a thin CLI executable target + a test executable
  target.
- **Directory-to-serve**: an explicit, required, absolute path - no
  "relative to cwd" or "relative to the binary" magic, since neither
  concept maps cleanly onto how an iOS app bundle locates its own resources
  later. The CLI wrapper resolves whatever the user types into an absolute
  path before handing it to the library.
- **Configuration surface**: CLI wrapper supports both `--host`/`--port`/
  `--dir` flags and `HOST`/`PORT`/`NATIVE_SERVER_DIR` env vars (`HOST`/`PORT`
  match `native-exp-server`'s existing convention; `NATIVE_SERVER_DIR` is
  new so it can't be confused with an unrelated `DIR` env var). Precedence:
  CLI flag > env var > default. Defaults: host `0.0.0.0`, port `8000`
  (matches today's documented command); no default for the directory - it's
  required, and the CLI exits with a clear usage error if it's missing.
- **Error response format**: JSON bodies (`{"error": "..."}`), matching
  `native-exp-server`'s existing convention, since this binary will host
  more JSON API routes once step 3 lands - one consistent shape from the
  start rather than switching formats mid-migration.
- **Scope boundary**: no `/health` endpoint in this step - staying strictly
  to static-file serving; `/health` arrives with step 3 as originally
  scoped, not pulled forward.
- **Test fixtures**: a small synthetic fixture directory
  (`native-server/tests/fixtures/`), not the real Vite `dist/` output -
  keeps tests hermetic and independent of the frontend having been built.
- **Build coupling**: fully decoupled from the frontend build. `native-
  server` doesn't know or care that its input directory usually comes from
  `vite build` - it just serves whatever absolute path it's given. No CMake
  step invokes `npm run build`.
- **Target platforms**: macOS + Linux are the only platforms actually built
  and tested in this step - no iOS toolchain, no cross-compilation, no
  device/simulator build yet. "Keep iOS in mind" for *this* step means
  something narrower and cheaper: only pick libraries/APIs already known to
  work on iOS (cpp-httplib and nlohmann/json are both plain portable C++
  with no macOS/Linux-only assumptions; Catch2 is test-only and never
  ships), and keep the library/CLI split so nothing CLI-only (signal
  handlers, env/flag parsing) leaks into the embeddable core. Actually
  building/embedding for iOS is real, separate work - see the new step
  described in "Roadmap change" below.
- **Concurrency**: a bounded worker-thread pool handling requests (blocking
  I/O per worker, not one-thread-total and not a hand-rolled async/event
  loop) - explicitly sized and configured, not left to an implicit library
  default. Genuine async I/O (Asio/Beast) was considered and explicitly
  rejected for this step - meaningfully more implementation complexity for
  a workload this small (one browser tab loading a handful of static
  assets, at most two devices at once), and the bounded pool already avoids
  the actual failure mode worth worrying about (unbounded thread growth
  under load).
- **CI**: held off for now - build/test locally for this step; add a
  GitHub Actions workflow for it later, independent of the Pages-deploy
  workflow (which is removed in a later step regardless).

## Design

### Repository layout

Revised during implementation to one CMake target per directory (each
library owns its own `include/` + `lib/`; binaries get neither), rather
than one shared `src/`+`include/` pair - see `CODING_STYLE.md` (written
during this step) for the exact rule and the naming conventions used
throughout:

```text
native-server/
  CMakeLists.txt                 # project(), FetchContent, add_subdirectory() x4
  CODING_STYLE.md                # naming + project-structure conventions
  README.md
  native_server_core/
    CMakeLists.txt
    include/native_server/server.h
    lib/server.cpp
  native_server_cli_lib/
    CMakeLists.txt
    include/native_server_cli/config.h
    lib/cli_config.cpp
  native_server_cli/
    CMakeLists.txt
    main.cpp
  tests/
    CMakeLists.txt
    fixtures/
      index.html
      manifest.webmanifest       # exercises the MIME-type-mapping fix
      nested/asset.txt
    unit/test_config.cpp
    integration/test_static_serving.cpp
```

No `cmake/ios.toolchain.cmake` in this step - that belongs to the new
Xcode-WIP step (see "Roadmap change" below) where an iOS build actually
happens; nothing in this step's own build needs it.

### Public library API (`include/native_server/server.h`)

Pimpl'd class so the public header - and anything linking against this
library later, including the eventual Xcode target - never needs to see
cpp-httplib's or nlohmann/json's headers, just this one:

```cpp
namespace native_server {

struct Options {
  std::string host = "0.0.0.0";
  uint16_t port = 8000;
  std::filesystem::path root_dir;   // must be set; absolute; must exist
};

class Server {
 public:
  explicit Server(Options options);   // throws ServerError if root_dir invalid
  ~Server();                          // stops if still running

  void start();                       // synchronous: returns only once bound
                                       // and actually listening, or throws
  void stop();                        // idempotent
  bool is_running() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class ServerError : public std::runtime_error { ... };

}  // namespace native_server
```

Deliberate embeddability constraints (so the native-app step doesn't need
to fight this code later): the library **never** installs process-wide
signal handlers and **never** calls `exit()`/`abort()` anywhere - all of
that is exclusively the CLI wrapper's job. `start()`/`stop()` are the only
lifecycle control an embedder (CLI today, the iOS app later) needs.

### `start()`/`stop()` mechanics

cpp-httplib's `listen()` is blocking, so `Impl::start()`:

1. Validates `root_dir` (exists, is a directory) - throws `ServerError`
   synchronously if not.
2. Calls `svr_.bind_to_port(host, port)` **on the calling thread** - if this
   fails (port in use, etc.), throws immediately, so a caller (or a test)
   knows right away whether startup actually succeeded, rather than
   discovering it asynchronously.
3. Only once bound, spawns a background thread running
   `svr_.listen_after_bind()`.
4. `stop()` calls `svr_.stop()` and joins that thread. Safe to call more
   than once (idempotent) and safe to `start()` again afterward - relevant
   since the native app will likely start/stop this across its own
   foreground/background lifecycle later.

### Concurrency model

The background thread from step 3 above is only the **accept loop** - it's
not the thread that handles requests, and this is not a single-threaded
server. cpp-httplib dispatches every accepted connection to its own
internal worker-thread pool (bounded, blocking I/O per worker) - a
thread-per-connection-from-a-bounded-pool model, not a hand-rolled
async/non-blocking event loop (no epoll/kqueue).

Made explicit rather than left as an implicit library default:

```cpp
constexpr std::size_t kThreadPoolSize = 8;   // deliberately fixed, not
                                              // hardware_concurrency()-based
svr_.new_task_queue = [] { return new httplib::ThreadPool(kThreadPoolSize); };
```

A fixed, small, documented pool size instead of a platform-dependent
default (cpp-httplib's own default scales with `hardware_concurrency()`) -
more predictable behavior across dev machines and iOS, and this app's real
concurrency never comes close to needing more. Revisit the number only if
a real bottleneck shows up, not preemptively.

### Static file serving

Leans on cpp-httplib's built-in `set_mount_point("/", root_dir)` rather than
hand-rolling path resolution/traversal-guarding/MIME lookup - the library
already handles index.html-on-`/`, per-extension MIME types for everything
this app's `dist/` output contains, and rejecting paths that would escape
the mount root. Two things added on top:

- `set_file_extension_and_mimetype_mapping(".webmanifest",
  "application/manifest+json")` - the one gap found by inspecting the
  actual manifest file; not in cpp-httplib's default MIME table.
- `set_error_handler(...)` - converts any non-2xx response (404 for a
  missing file, etc.) into a JSON body via nlohmann/json, matching the
  project-wide error-shape decision above.

No SPA fallback routing (no `index.html`-for-any-unknown-path behavior) -
the current app has no client-side router and no deep-linkable routes (see
`B-current-architecture.md`), so an unknown path should keep returning a
real 404, matching today's actual behavior exactly rather than introducing
new behavior.

"Light router now": cpp-httplib's `Server` object *is* the router -
registration happens in one clearly-named private method (e.g.
`Impl::register_routes()`), called from `start()` before binding, so step 3
can add `svr_.Get("/health", ...)` etc. right there without restructuring
anything. Not a custom router abstraction on top of cpp-httplib - that
would be building for a hypothetical rather than the already-committed
step 3.

### CLI wrapper (`src/main.cpp`)

- Tiny hand-rolled flag parser (no library needed - three flags).
- Resolves config via CLI flag > env var > default (see above); prints a
  clear usage error and exits non-zero if no directory is resolvable.
- Resolves a relative `--dir`/`NATIVE_SERVER_DIR` value to an absolute path
  (`std::filesystem::absolute`) before constructing `Options`.
- Prints a startup line mirroring both existing Python servers' style,
  e.g. `native-server listening on http://0.0.0.0:8000, serving
  /path/to/dist`.
- Per-request logging to stdout via `httplib::Server::set_logger(...)`, in
  the same `<addr> - "<method> <path>" <status>` shape both existing Python
  servers already use.
- Installs `SIGINT`/`SIGTERM` handling for graceful shutdown (calls
  `stop()`, exits 0) - mirrors Python's `KeyboardInterrupt` handling. This
  lives only in the CLI wrapper, never in the library (see embeddability
  constraint above).

### Dependencies (CMake `FetchContent`, pinned tags)

cpp-httplib, nlohmann/json, Catch2 (v3, using its `Catch2::Catch2WithMain`
target). `native_server_core` (a small static library) links cpp-httplib +
nlohmann/json; `native_server_cli` links only `native_server_core`; the
test executable links `native_server_core` + `Catch2::Catch2WithMain`.
C++17 (sufficient for `std::filesystem`; no need for C++20 here).

### iOS - what this step does and doesn't do

Deliberately narrow scope here: this step only makes iOS-friendly *choices*
(library selection, library/CLI split) - it does **not** stand up an iOS
toolchain, cross-compile, or verify a device/simulator build. That real
verification (does `native_server_core` actually compile/link for iOS)
belongs to the new step described in "Roadmap change" below - a minimal
Xcode+WebView WIP template - inserted before the full native-app step, so
it's proven in isolation rather than assumed all the way until then.

### Tests

Scope deliberately fixed at the four categories below - no binary-content-
integrity test, no CLI-binary-as-subprocess test tier, no periodic real-
`dist/` smoke test, no fuzzing, no lifecycle soak/leak-detection loop - all
considered and explicitly rejected as more than this project needs.

- **Unit** (`tests/unit/test_config.cpp`): CLI/env parsing precedence,
  missing-directory error, invalid-port handling - no real sockets.
- **Integration - basic black-box HTTP** (`tests/integration/`), against
  `tests/fixtures/`, using cpp-httplib's own `httplib::Client` against a
  real running instance on an ephemeral test port:
  - `GET /` → 200, serves `fixtures/index.html`.
  - `GET /nested/asset.txt` → 200, correct body.
  - `GET /manifest.webmanifest` → 200, `Content-Type:
    application/manifest+json`.
  - `GET /does-not-exist` → 404, JSON body `{"error": "Not found"}`.
  - `start()` → reachable → `stop()` → connection now refused → `start()`
    again on the same `Server` → reachable again (one lifecycle cycle, not
    a repeated soak loop).
- **Integration - security edge cases**: a plain `../`-style request, a
  percent-encoded variant (`%2e%2e%2f`), and - the sharper case, distinct
  from a simple path-string check - a symlink inside `tests/fixtures/`
  pointing outside it, confirmed rejected in all three cases rather than
  assuming `set_mount_point`'s guard covers the symlink case just because
  it covers the string case.
- **Integration - protocol conformance**: `POST`/`PUT`/`DELETE` against a
  static file path → 405 (not something undefined); a `HEAD` request's
  actual behavior confirmed (cpp-httplib's mount point may already handle
  it); and confirming what caching headers (`ETag`/`Last-Modified`), if
  any, actually show up on a plain `GET` - documented by a test either way,
  not left as an assumption, even though this project isn't deliberately
  implementing caching.
- **Integration - concurrency load**: fire more concurrent requests than
  `kThreadPoolSize` (8) at the running server and confirm every one
  completes successfully (no failures, no hangs) - verifies the pool
  actually queues/serves under load rather than dropping or blocking
  requests past its worker count. A single load test, not a repeated
  soak/leak-detection loop.
- Wired into `ctest` (`catch_discover_tests` or plain `add_test`).

### Docs touched as part of this step

- New `native-server/README.md` (build/run commands, CLI flags/env vars,
  explicit "no HTTPS, no companion-server duty yet" scope note).
- Root `README.md`'s "Test locally" section: add `native-server` as the new
  way to serve `dist/`, alongside (not replacing) `native-exp-server`'s
  existing Python instructions, which are untouched until step 3/4.
- `C-migration-steps.md`: mark step 2's status, add implementation notes.

### Explicitly out of scope for this step

- `/health` endpoint (step 3).
- Folding in `native-exp-server`'s caption-fetching logic (step 3).
- Deleting either existing Python server (step 4).
- Any actual Xcode project, app UI, or iOS toolchain build (see "iOS - what
  this step does and doesn't do" above) - this step only makes iOS-friendly
  library/structure choices; real iOS build verification and embedding
  belong to the new step below.
- HTTPS/TLS support in the server itself (the app's existing Cloudflare-
  tunnel-for-HTTPS approach, and iOS's `localhost`-is-secure-context
  exemption, both remain unchanged).
- CI workflow.

## Verification

1. `cmake -S native-server -B native-server/build && cmake --build native-server/build`
2. `ctest --test-dir native-server/build --output-on-failure` - all unit +
   integration tests pass, including the traversal-rejection and
   start/stop-lifecycle cases.
3. Manual smoke test against the real app: `npm run build`, then
   `./native-server/build/native_server_cli --dir "$(pwd)/dist" --port 8000`,
   open `http://localhost:8000` in a browser and confirm the app loads and
   behaves identically to the current `python3 -m http.server` setup.
4. `curl -I http://localhost:8000/manifest.webmanifest` shows
   `Content-Type: application/manifest+json`.
5. `curl http://localhost:8000/../../../etc/passwd`-style requests (and a
   percent-encoded variant) confirm rejection, not real file contents.
6. Confirm `Ctrl+C` shuts the CLI down cleanly (no hung process, port is
   free immediately after).

## Roadmap change: new step inserted before the native-app step

Also agreed while planning this step: insert a new step into
`C-migration-steps.md` (and the checklist in `A-vision-and-goals.md`),
between the current "native-server" work (steps 2-4) and the original
"create a new native Xcode app that serves that server locally + displays
the app as a webview" step:

> **New step: minimal iOS Xcode WIP/template.** A real, working (if
> rough-edged) Xcode app: builds `native-server`'s core library for iOS for
> real (device + simulator, via `leetal/ios-cmake` - the toolchain
> verification originally drafted for step 2, moved here instead), embeds
> it in-process on a background thread bound to `127.0.0.1`, and a
> `WKWebView` loads `http://127.0.0.1:<port>` serving the real built
> `dist/` bundled into the app. **Mic access must work end-to-end at this
> stage** - `NSMicrophoneUsageDescription` in `Info.plist` and the
> `WKUIDelegate` media-capture-permission callback are implemented now, not
> deferred - so Recorder Loop, Robot Shadowing, Dictionary, and YT
> Shadowing's Record/Shadow/Auto-Shadow/Capture must all work at 100%, the
> same as they do today in a browser. The **only** thing that's expected
> not to work at this stage is anything needing `native-exp-server`
> (YT Shadowing's CC/subtitle sidebar) - that Python companion process
> isn't reachable from a standalone phone and isn't folded into
> `native-server` until step 3. Everything else - including Cloudflare-
> Worker-backed history sync, which just needs the phone's own internet
> connection, nothing local - is real scope for this step, not a
> stripped-down demo.

This pushes the original "native Xcode app" step one slot later in the
numbered list (it now builds on top of this new step's proven, working
shell rather than starting from nothing).

### Notes on iOS secure-context/mic access (learned while planning)

- `http://localhost` (or `127.0.0.1`) already counts as a secure context to
  WebKit (same engine as Safari) - this is *why* mic access already works
  on a laptop today over plain HTTP, and exactly why the Cloudflare tunnel
  is only needed when a *different device* hits the laptop's LAN IP. Once
  the server runs on-device and the WebView loads `localhost`, no TLS is
  ever needed for this reason.
- Separately, `WKWebView` itself needs native-side wiring to allow media
  capture at all, regardless of origin security - `NSMicrophoneUsageDescription`
  in `Info.plist`, and implementing `WKUIDelegate`'s
  `webView(_:requestMediaCapturePermissionFor:initiatedByFrame:type:decisionHandler:)`
  (available iOS 15+; this app already assumes 16.4+, so no version risk).
  Without both, `getUserMedia()` fails even on `localhost`. Not a step 2
  concern, but written down here so it isn't rediscovered the hard way in
  the iOS-WIP step.
- When the server eventually runs *inside* the iOS app, it should bind to
  `127.0.0.1` specifically (not `0.0.0.0`) - loopback-only avoids ever
  triggering iOS's "Local Network" permission prompt, which applies to
  LAN-wide/Bonjour-style access, not pure loopback. The CLI's `0.0.0.0`
  default remains correct for today's laptop/dev use; this only matters
  once embedded.

## Implementation notes (post-hoc)

Status: **done** - built, all tests passing, verified manually against the
real `npm run build` output. Deltas from the plan above, discovered while
actually building it:

- **Project structure** changed mid-implementation to one CMake target per
  directory (library targets get their own `include/` + `lib/`; the CLI
  binary gets neither, since nothing links against it) - see the revised
  layout above and `native-server/CODING_STYLE.md`.
- **Naming convention** (`native-server/CODING_STYLE.md`, new): `PascalCase`
  types, `camelCase` functions/variables/public members, `_camelCase`
  private members, `kPascalCase` constants. Chosen mid-implementation,
  applied throughout (`ServerOptions`, `isRunning()`, `_impl`, `resolveConfig`,
  `kThreadPoolSize`, etc.) - a standing convention for any future C++ in this
  repo, not just this step.
- **cpp-httplib's `CPPHTTPLIB_LISTEN_BACKLOG` defaults to 5** - far smaller
  than `kThreadPoolSize` (8), let alone the concurrency-load test's 40
  simultaneous connections. Confirmed by reading cpp-httplib's own source
  (not assumed): a burst past 5 pending connections gets refused/reset by
  the OS accept queue *before* ever reaching the application-level thread
  pool - the exact "drops instead of queues" failure mode the concurrency
  test was written to catch, just one layer lower (OS socket backlog, not
  the thread pool) than originally anticipated. Fixed by defining
  `CPPHTTPLIB_LISTEN_BACKLOG` to 64 before including `<httplib.h>` in
  `server.cpp` - both settings (thread pool size and accept backlog) need
  to be sized coherently for the "queues rather than drops" guarantee to
  actually hold end-to-end. 10/10 clean test runs after the fix.
- **`set_mount_point` return value is checked** (`bool`, per cpp-httplib's
  own canonical example) as defense-in-depth alongside this project's own
  `std::filesystem::is_directory` pre-check, rather than only trusting one
  of the two.
- **Non-GET/HEAD methods on a static path return 404, not 405** - confirmed
  by reading cpp-httplib's routing source directly: with no handler
  registered for `POST`/`PUT`/`DELETE`, it has no notion of "path exists but
  wrong method," so it falls through to the same 404 an unmatched path
  would get. Accepted as-is (matches this project's "test actual behavior,
  don't assume" plan for this category) rather than adding custom logic to
  produce a 405 nothing in this app actually needs.
- **Request logging** ended up as an opt-in `ServerOptions::enableStdoutLogging`
  field rather than exposing `httplib::Server::set_logger` directly - keeps
  `httplib` fully hidden behind the pimpl even for this CLI-requested
  feature; the CLI sets it `true`, an embedder (the future iOS app) can
  leave it `false`.
