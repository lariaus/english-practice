# App Overview: English Pronunciation Practice App

## What this app is

A small collection of self-practice "tools" for improving English pronunciation:
**Recorder Loop** (see `pronunciation-self-monitor-spec.md`), **Robot
Shadowing** (see `robot-shadowing-spec.md`), **YT Shadowing** (see
`yt-shadowing-spec.md`), and **Flashcards** (see `flashcards-spec.md`),
designed to accommodate more tools later without rework. **Dictionary**
(see `dictionary-spec.md`) is a different shape of "tool" - a global popup
reachable from anywhere, not a screen you navigate to (see below).

This doc covers the app shell: navigation structure, tech stack, and local
development/testing workflow. Tool-specific behavior lives in its own doc file
(one per tool). Interaction patterns shared *across* tools (not owned by any
one of them) live in `common-design-philosophy.md`.

## App Structure / Navigation

- **Home screen**: a menu listing available tools (a simple list of
  buttons/cards) — currently **"Recorder Loop"**, **"Robot Shadowing"**,
  **"YT Shadowing"**, **"Flashcards"**, and **"Dictionary"**.
- **Tool screen**: reached by tapping a tool on the home screen.
  - Has its own **Back** button to return to the home screen.
  - **Recorder Loop** first shows a duration picker — a row of buttons for
    **5s / 10s / 15s / 30s / 60s / 90s** — before showing its loop UI.
  - **Robot Shadowing** has no duration picker; instead it shows session
    options (voice, phrase database/difficulty, repeat count, repeat model
    toggle) before Start/Stop — its response window is derived per-phrase,
    not user-chosen (see its own spec doc for details).
  - **YT Shadowing** first shows a URL-entry/history screen, then a second
    screen with the video player and all controls — see its own spec doc
    for details.
  - **Flashcards** first shows a set picker, then a per-set screen (view/
    edit cards, with Learn/Review/Practice sub-modes reached from there) —
    see its own spec doc for the full navigation and its offline-read-only
    fallback.
  - **Dictionary** is the one exception to "tapping a tool reaches a new
    screen" — it opens a popup overlay on top of whatever's currently
    showing (Home included) instead of navigating anywhere. It's also
    reachable without ever visiting Home at all: a global keyboard shortcut
    opens it from any screen, and YT Shadowing's transcript-word click opens
    it pre-filled with that word — see `dictionary-spec.md`.
- Routing between Home and tool screens: no full router library needed for
  now — a tiny reactive "current screen" value is enough given how few
  screens exist. Revisit only if the number of tools/screens grows a lot.

## Tech Stack & Architecture

- **Framework: Vue 3** using `<script setup>` (Composition API), built with
  **Vite**. No backend, no server-rendered anything — pure client-side SPA,
  static output deployable to GitHub Pages.
- **Why Vue**: the app is view-light and API-heavy — most of the complexity
  is in `getUserMedia` / `MediaRecorder` / Wake Lock orchestration, not in UI
  composition. Vue is small (~15-20KB gzipped runtime) and doesn't force any
  data-flow philosophy onto non-UI code — only the handful of values the
  templates actually need to react to (current phase, seconds remaining,
  selected duration, current screen) are wrapped in `ref()`/`reactive()`.
  Everything else (MediaRecorder instances, MediaStream, timers,
  WakeLockSentinel) is plain JS, untouched by the framework.
- **Engine / view separation** (applies to every tool, not just Recorder
  Loop): each tool's core logic (state machine, timers, audio APIs,
  permissions) lives in plain, framework-agnostic JS modules/classes — "the
  engine." Vue components are a thin binding layer on top: they call into the
  engine and reflect its state, they don't own the audio logic themselves.
  This keeps the hard part portable/testable and means the framework choice
  never blocks future engine changes.
- **Build step accepted**: earlier drafts of the Recorder Loop spec said "no
  build step ideally" — that's superseded now that we're using Vue SFCs,
  which require a Vite build. Accepted tradeoff for better long-term
  architecture. Output of `vite build` is a plain static `dist/` folder,
  which is what gets deployed to GitHub Pages.

## Local Development & Testing

Two ways to actually run and test the app, both starting from the same
`vite build` static `dist/` output — the Vite **dev server** (HMR/module-
transform) is not the day-to-day way this gets tested on-device either way:

- **Browser** (laptop or a phone's browser): serve `dist/` with
  `native_server_cli` (see `native-server/README.md` and the root
  `README.md`) — a real C++ server, not a generic static file server, since
  it also answers `/subtitles` (YouTube captions) and `/storage/maps/...`
  (see `docs/local-storage.md`) alongside plain file serving. Reachable
  from a phone's browser over the local network (same Wi-Fi) by binding to
  the LAN interface (`--host 0.0.0.0`), not just `localhost`.
  - **Resolved gotcha**: iOS Safari only allows `getUserMedia` (mic access)
    in a "secure context." `localhost` counts as secure automatically (why
    laptop testing works over plain HTTP), but a phone hitting the laptop's
    LAN IP does not — confirmed by testing, Safari blocks mic access there.
    Solved with a free Cloudflare quick tunnel (`cloudflared`) to get a real
    `https://` URL without deploying anywhere — see the README for the
    exact steps.
- **The real Mac/iOS app** (`english-practice-app/`, run via Xcode): embeds
  `native-server` directly in-process, bound to `127.0.0.1` only — no LAN
  exposure and no tunnel needed for this path, since the `WKWebView` and
  the server it's talking to are both local to the same device/process.

## Hosting (production)

- **GitHub Pages** — free static hosting, HTTPS by default, no server
  maintenance. A GitHub Actions workflow (`.github/workflows/deploy.yml`)
  builds and publishes `dist/` automatically on every push to `main` — see
  the README for the one-time repo setting and the live URL.
- "Add to Home Screen" support is in place: a web app manifest, Apple meta
  tags, and a generated icon set (`public/icons/`), so it launches
  standalone on iOS (full-screen, own icon, no Safari chrome).

## Backend & local storage

Two independent, deliberately separate systems now exist (both previously
"out of scope," since built):

- **`native-server`** (C++) — serves this app's own static files, YouTube
  captions, and small per-device key-value storage (`StorageMap`), either
  standalone (`native_server_cli`, for local dev/testing per above) or
  embedded directly in the Mac/iOS app. See `docs/local-storage.md` and
  `native-server/README.md`.
- **Cloudflare Worker + KV** — cross-device-*synced* state: YT Shadowing
  history (small, today), and Flashcards' sets/cards (its actual data
  store - the one case here that's more than "small state," see
  `flashcards-spec.md`) - a separate tier from the local-only storage
  above. See `common-design-philosophy.md`'s "Online shared storage"
  section and `docs/cross-device-sync.md`.

Flashcards is the one feature that uses *both* tiers at once: the Worker
is its real source of truth (always tried first), and `StorageMap` backs
a minimal read-only fallback (front/back content only, no scheduling
state) for when there's no connection - see `flashcards-spec.md`'s
"Offline read-only cache" section.

## Explicitly out of scope for now

- Accounts (still no user/login concept anywhere in the app).
- A full router library, state management library (Vuex/Pinia), or other
  infrastructure not yet justified by the app's current size.
- Service worker caching for the GitHub-Pages-hosted web build — decided
  not needed; offline support is instead pursued via the native Mac/iOS
  app (see "Backend & local storage" above), not a web-platform PWA
  caching layer.
