# Vision & Goals

## The destination (still somewhat abstract)

Turn this from "a static SPA you deploy to GitHub Pages, optionally paired
with two hand-started local Python helper processes" into a real native app
that works offline, and stays fully in sync across a laptop and an iPhone.

## The ordered checklist

Given by the user as the starting point for this migration; kept here
verbatim as the source of truth for ordering, expanded with notes/decisions
as they're made. See `C-migration-steps.md` for live status tracking of
each item - this list is the plan, that doc is the log.

1. [x] Set up a Cloudflare sync system to share data between iPhone/laptop -
   **done already**, pre-dates this migration. See
   `B-current-architecture.md` and `docs/cross-device-sync.md`.
2. [ ] Rewrite the static Python server with a C++ static server.
3. [ ] Replace the "companion exp server" (`native-exp-server/`, Python)
   with an extension of the new C++ static server (not a separate Python
   process anymore).
4. [ ] Remove both old Python servers, using only the new C++ server.
5. [ ] Minimal iOS Xcode WIP/template: `native-server` built for iOS for
   real (device + simulator), embedded in-process, a `WKWebView` serving
   the real app from `http://127.0.0.1:<port>` - mic access must work
   end-to-end at this stage. Only `native-exp-server`-dependent features
   (CC/subtitles) are expected not to work yet. Inserted here, before the
   full native-app step, specifically to de-risk iOS embedding and mic
   access in isolation rather than assuming both work until step 6. See
   `step-2.md`'s "Roadmap change" section for the full reasoning.
6. [ ] Create a new native Xcode app that serves that C++ server locally
   and displays the app as a webview on top of it (building on step 5's
   proven shell).
7. [ ] Make it possible to fully test 100% of the app directly via
   Cloudflare tunneling alone (i.e. without needing the native app wrapper
   to exercise everything).
8. [ ] Get rid of the GitHub Pages deploy and static-serve hosting model.
9. [ ] Replace all `localStorage` use cases with a new in-server storage.

Steps 2-4 are one connected arc (Python servers → one C++ server). Step 5
is a deliberately narrow iOS-embedding proof (not the real app). Step 6 is
the native shell built on top of it. Step 7 is a testability guarantee that
has to keep being true throughout, not a one-time task at the end. Steps
8-9 are the final cleanup once everything upstream of them is solid.

## Guiding principles (clarified in discussion, 2026-07-28)

### 1. "Offline" is a gradient, not a single switch

Not every tool can be offline - some are fundamentally network-shaped:

- **Recorder Loop** - already 100% offline today (pure mic in/out, no
  network calls at all). The one tool with nothing to change here.
- **YT Shadowing** - can **never** be fully offline; it's built around
  streaming a real YouTube video via the IFrame Player API. Offline work for
  this tool is about everything *around* the video (captions/subtitles,
  history, capture state) working locally even when the video itself
  requires a network - not about the video playing with no connection.
- **Robot Shadowing** / **Dictionary** - currently need network (Google
  Translate TTS, api.dictionaryapi.dev) but aren't fundamentally tied to it
  the way YT Shadowing is - as the in-server storage layer grows (step 9 and
  beyond), more of their data/audio can move to being cached/served locally,
  incrementally reducing their network dependency over time.

The intent: **push the offline-capable surface area larger over time**, not
flip a single "offline mode" switch. Each step that adds local storage is a
chance to make one more thing not require a live connection, not a
one-shot binary goal.

### 2. Two storage tiers, kept genuinely separate

Cloudflare KV (via the Worker) and the new C++ server's local storage are
**not** layers of the same system - they solve different problems and
should keep doing so:

- **Cloudflare KV**: small, frequently-changing, must-look-the-same-
  everywhere state - e.g. the last-5-YT-videos history. Stays exactly as
  it is (see `docs/cross-device-sync.md`, `docs/common-design-philosophy.md`
  "Online shared storage"). Not being replaced or subsumed by the new server
  storage.
- **New C++ server-side storage**: bulkier, more local-feeling data that
  doesn't need to be tiny or globally synced the same way - e.g. a full
  library of fetched subtitle files. Local to wherever the C++ server is
  running (in practice: the native app's device, or whatever machine the
  server is on).
- Consequence: step 9 ("replace all localStorage") is about moving
  `localStorage`-shaped data to *this* new tier, not to Cloudflare KV. The
  one `localStorage` key that exists today (`sync-server-url`, see
  `B-current-architecture.md`) is a special case worth thinking about
  separately, since it's the bootstrap value needed *to find* the sync
  server in the first place - can't be moved into a server-side store that
  itself needs this value to be reachable.
- Per the checklist ordering, the C++ server's *storage* capability is a
  **late-stage** addition - it comes only after the server already exists
  and is doing static-file-serving + caption-fetching duty (steps 2-4). Do
  not pull storage work forward into the earlier server-rewrite steps.

### 3. Unit tests from day one, not bolted on later

Decided explicitly during planning: the new C++ server (and any other new
component built during this migration) gets unit test coverage as it's
built, not retrofitted afterward. This matters most for the caption-
fetching reimplementation (step 3) specifically, since it's inherently the
riskiest piece of the whole migration - see "Caption fetching in C++" in
`B-current-architecture.md` and the open questions doc for why.

## Explicitly acknowledged risk

Reimplementing YouTube caption/subtitle fetching in C++ (replacing Python's
`youtube-transcript-api`) is expected to be genuinely painful - it's
reverse-engineering an unofficial, undocumented endpoint with no client
library to lean on in C++. Accepted as necessary rather than deferred
indefinitely, specifically *because* it's necessary to fully retire the
Python side (step 4) - see `D-open-questions.md` for what's still unsettled
about how to approach it.
