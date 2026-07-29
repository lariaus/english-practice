# Open Questions

Things not yet resolved. Check here before assuming a decision has been
made - move an item to the relevant step's notes in `C-migration-steps.md`
(and/or into `A-vision-and-goals.md` if it's a standing principle) once
it's actually decided, and delete it from here.

## Caption fetching in C++ (step 3)

Reimplementing what `youtube-transcript-api` does is the single riskiest
piece of this migration - reverse-engineering an unofficial, undocumented
YouTube endpoint, in a language with no existing client library for it.
Agreed this has to happen (no way around it, per the checklist), and that
it gets unit tests from the start. Still unresolved:

- What HTTP/TLS client library to use in C++ for this (needs to fetch HTML
  pages and JSON endpoints, handle redirects, parse out embedded JSON blobs
  the way `youtube-transcript-api` does).
- What the unit test strategy actually looks like given the target is a
  live, unofficial, undocumented external endpoint: recorded/fixture-based
  responses (stable, fast, safe for CI, but can silently drift from reality
  if YouTube changes something) vs. real network calls in tests (catches
  real breakage, but flaky/slow/rate-limit-risk in CI). Likely some mix, not
  yet decided.
- Whether to port `youtube-transcript-api`'s exact approach/endpoints
  1:1, or research the current state of YouTube's caption delivery
  fresh (the Python library's approach may itself be outdated or have
  since found a more robust path).
- Whether there's a transition period where the C++ server shells out to
  the existing Python script for captions specifically (keeping one Python
  dependency alive temporarily) while the C++ implementation is built and
  proven, or whether it's a hard cutover once ready. Leaning toward
  "prove it in C++ before touching step 4," but not explicitly decided.

## C++ server's static-storage scope (steps 3 vs. the later storage work)

`A-vision-and-goals.md` principle #2 says server-side storage is a
late-stage addition, separate from Cloudflare KV. Not yet decided:

- Exact data model/format for that storage (flat files? SQLite? something
  else?) - deliberately deferred until that stage per the plan, but noting
  it here so it isn't forgotten as "TBD," not "decided."
- Whether the subtitle-fetch results (step 3) get cached in this storage
  layer as soon as it exists, or subtitles stay a pure fetch-on-demand
  passthrough even after storage exists.

## The bootstrap problem for `sync-server-url` (step 8)

The one real `localStorage` key today (the Cloudflare Worker's URL) exists
specifically so the client knows *where* the sync server is. If step 8
moves this into "in-server storage," that only works once there's already
a local server to ask - which is true once steps 2-5 land (the native app
always runs a local C++ server). But:

- Does this setting become "ask the local C++ server, which itself
  remembers the Worker URL" (server-side config file, one per device/
  install), or does it stay a thin client-side setting because it's
  fundamentally per-install config rather than "data," and only the actual
  *data* (step 8's real target) moves? Not yet decided - flagged in
  `B-current-architecture.md` as worth thinking about separately rather
  than assuming it's an automatic part of step 8's scope.

## Native app / webview tech choice (step 5)

Not yet discussed at all: WKWebView-based wrapper (Swift/SwiftUI shell +
webview pointed at the local C++ server), vs. something else. No decision
made, no research done yet.

## Testing strategy for the C++ server generally (not just captions)

Agreed (per `A-vision-and-goals.md` principle #3) that unit tests are
written from day one for the new C++ server and other new components. Not
yet decided:

- Test framework/tooling choice (e.g. GoogleTest, Catch2, doctest).
- Whether/how to structure integration-style tests (spin up the real server
  on a test port, hit it with real HTTP requests) alongside pure unit tests
  of individual functions.
- Whether this project sets up any CI to run these automatically, or tests
  are run manually for now (repo currently has GitHub Actions only for the
  Pages deploy, which itself is being removed in step 7).
