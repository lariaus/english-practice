# Migration Steps — live status log

Tracks the ordered checklist from `A-vision-and-goals.md` as work actually
happens. Update status/notes here as steps start, progress, and complete -
this is the log; that doc is the plan.

Status legend: `not started` / `in progress` / `blocked` / `done`.

---

## 1. Cloudflare sync system (iPhone ⇄ laptop)

**Status: done** (pre-dates this migration).

See `B-current-architecture.md` "Cloudflare sync" and
`docs/cross-device-sync.md` for the full existing pipeline. Nothing to do
here unless a later step needs to extend it (per
`A-vision-and-goals.md` principle #2, extending it follows the existing
"add a KV key + a Worker route" pattern, not a rework).

## 2. Rewrite the static Python server as a C++ static server

**Status: done.** Full implementation plan + post-hoc implementation notes
in `step-2.md`.

Scope: replace `python3 -m http.server 8000 --directory dist` with a small
C++ static file server (`native-server/`) - bind `0.0.0.0`, serve `dist/`
as-is. No storage duty yet (see principle #2 - storage is a late-stage
addition, not part of this step).

Notes / decisions: see `step-2.md` for the full writeup. Summary:
cpp-httplib, CMake, Catch2, and nlohmann/json (all via `FetchContent`); a
library-plus-thin-CLI structure from day one (not a monolithic `main()`)
since iOS can't spawn subprocesses and will need this embedded in-process
later; iOS itself gets only library-choice consideration in this step, not
an actual toolchain build (that's the new step 5 below); a bounded,
explicitly-sized thread pool (not async I/O, not an implicit default); JSON
error bodies throughout, matching `native-exp-server`'s convention ahead of
step 3.

## 3. Replace the companion exp server with an extension of the C++ server

**Status: not started - deferred until after step 5** (the iOS WIP
template is being tackled first; no dependency forces this ordering, it's
just the order the user chose to work in).

Scope: fold `native-exp-server/`'s job (currently `GET /health`, `GET
/subtitles?url=...&lang=...`) into the same C++ binary from step 2, as
additional routes - not a second process anymore. Requires reimplementing
the caption-fetch logic currently provided by Python's
`youtube-transcript-api` package, in C++, from scratch (no equivalent
library) - see "Caption fetching in C++" below and `D-open-questions.md`.

Per decision in `A-vision-and-goals.md`: **unit tests for this are written
alongside the implementation, not after.**

Notes / decisions: (none yet)

## 4. Remove both old Python servers

**Status: not started. Blocked on step 3 reaching parity.**

Delete `native-exp-server/` and the README's `python3 -m http.server`
instructions once the C++ server fully replaces both. Also touches:
`native-exp-server/requirements.txt`, its `.venv`, `README.md`'s local-dev
instructions, `docs/app-overview.md`'s "Local Development & Testing"
section, `docs/yt-shadowing-spec.md`'s "NativeExpServer (companion process)"
section (per the standing instruction to keep that spec doc current as
features change).

Notes / decisions: (none yet)

## 5. Minimal iOS Xcode WIP/template

**Status: not started - up next, ahead of step 3.** Inserted during step
2's planning - see `step-2.md`'s "Roadmap change" section for the full
reasoning. Deliberately tackled before step 3 (captions) - this step was
already scoped to depend only on `native-server`'s static-serving
capability (done in step 2), not on captions being folded in, so there's
no ordering conflict in doing the iOS work first.

Scope: a real, working (if rough-edged) Xcode app - `native-server`'s core
library built for iOS for real (device + simulator, via `leetal/ios-cmake`),
embedded in-process on a background thread bound to `127.0.0.1`, a
`WKWebView` loading `http://127.0.0.1:<port>` serving the real built
`dist/` bundled into the app. **Mic access must work end-to-end at this
stage** (`NSMicrophoneUsageDescription` + the `WKUIDelegate` media-capture-
permission callback, both implemented now, not deferred) - Recorder Loop,
Robot Shadowing, Dictionary, and YT Shadowing's Record/Shadow/Auto-Shadow/
Capture must all work at 100%, same as in a browser today. The only thing
expected *not* to work at this stage is anything needing `native-exp-server`
(YT Shadowing's CC/subtitle sidebar) - not reachable from a standalone
phone, and not folded into `native-server` until step 3. Cloudflare-synced
history works fine (just needs the phone's own internet connection).

Purpose: de-risk iOS embedding + mic access in isolation, before the real
native-app step (6) builds on top of it.

Notes / decisions: see `step-2.md`'s "Notes on iOS secure-context/mic
access" for what was learned about `localhost`-as-secure-context and
`WKWebView`'s separate media-capture-permission requirement while planning
step 2.

## 6. Create the real native Xcode app (local server + webview)

**Status: not started. Builds on step 5's proven shell rather than starting
from nothing.**

Scope: a native app that launches the C++ server locally and displays the
app as a webview on top of it. Since the server runs on-device, `localhost`
is automatically a secure context on iOS - this is likely how mic access
stops needing the Cloudflare-tunnel workaround for the *native-app* path
specifically (the tunnel need documented in
`B-current-architecture.md` "Deploy/hosting model" was specifically about
testing in mobile Safari against a laptop's LAN IP, which won't be how this
works once there's a native wrapper).

Notes / decisions: (none yet)

## 7. Full testability via Cloudflare tunneling alone

**Status: not started (and re-checked continuously once started - see below).**

Scope: whatever the native app can do, it must remain possible to fully
exercise via a plain Cloudflare tunnel + browser too, no native app
required. This isn't a one-time task done once and forgotten - every step
from here on should be checked against "can this still be tested through a
tunnel alone" rather than only being validated through the native app.

Notes / decisions: (none yet)

## 8. Get rid of GitHub Pages deploy + static hosting

**Status: not started. Comes last - depends on the native app (step 6)
being the real distribution/usage model.**

Scope: remove `.github/workflows/deploy.yml`, the GitHub Pages hosting
model, and update `README.md` (currently states the live GitHub Pages URL
as the primary way to use the app).

Notes / decisions: (none yet)

## 9. Replace all `localStorage` with in-server storage

**Status: not started. Late-stage, per `A-vision-and-goals.md` principle #2 -
depends on the C++ server's storage layer existing, which itself comes
after steps 2-4 are solid.**

Scope, per current architecture (`B-current-architecture.md`): really just
one key today, `sync-server-url` (`src/engine/syncConfig.js`). Open question
on how a value needed to *find* the server can itself live in that same
server - see `D-open-questions.md`.

Notes / decisions: (none yet)
