# App Overview: English Pronunciation Practice App

## What this app is

A small collection of self-practice "tools" for improving English pronunciation:
**Recorder Loop** (see `pronunciation-self-monitor-spec.md`) and **Robot
Shadowing** (see `robot-shadowing-spec.md`), designed to accommodate more
tools later without rework.

This doc covers the app shell: navigation structure, tech stack, and local
development/testing workflow. Tool-specific behavior lives in its own doc file
(one per tool).

## App Structure / Navigation

- **Home screen**: a menu listing available tools (a simple list of
  buttons/cards) — currently **"Recorder Loop"** and **"Robot Shadowing"**.
- **Tool screen**: reached by tapping a tool on the home screen.
  - Has its own **Back** button to return to the home screen.
  - **Recorder Loop** first shows a duration picker — a row of buttons for
    **5s / 10s / 15s / 30s / 60s / 90s** — before showing its loop UI.
  - **Robot Shadowing** has no duration picker; instead it shows session
    options (voice, phrase database/difficulty, repeat count, repeat model
    toggle) before Start/Stop — its response window is derived per-phrase,
    not user-chosen (see its own spec doc for details).
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

- The Vite **dev server** (with its HMR/module-transform magic) is not the
  day-to-day way this app gets tested on-device. Instead: run `vite build`
  to produce the static `dist/` folder, then serve that folder with a
  **simple static file server** — no bundler, no middleware, just serves
  files as-is — so what's tested locally is the same static output that
  will be deployed to GitHub Pages.
- The static server must be reachable from **both a laptop browser and an
  iPhone** over the local network (same Wi-Fi), so it needs to bind to the
  LAN interface (`0.0.0.0`), not just `localhost`.
- **Resolved gotcha**: iOS Safari only allows `getUserMedia` (mic access) in
  a "secure context." `localhost` counts as secure automatically (why laptop
  testing works over plain HTTP), but an iPhone hitting the laptop's LAN IP
  does not — confirmed by testing, Safari blocks mic access there. Solved
  with a free Cloudflare quick tunnel (`cloudflared`) to get a real
  `https://` URL without deploying anywhere — see the README for the exact
  steps.

## Hosting (production)

- **GitHub Pages** — free static hosting, HTTPS by default, no server
  maintenance. A GitHub Actions workflow (`.github/workflows/deploy.yml`)
  builds and publishes `dist/` automatically on every push to `main` — see
  the README for the one-time repo setting and the live URL.
- "Add to Home Screen" support is in place: a web app manifest, Apple meta
  tags, and a generated icon set (`public/icons/`), so it launches
  standalone on iOS (full-screen, own icon, no Safari chrome).

## Explicitly out of scope for now

- Accounts, backend, or data storage beyond local/in-session (and possibly
  browser local storage) state.
- A full router library, state management library (Vuex/Pinia), or other
  infrastructure not yet justified by the app's current size.
- Offline support / service worker caching — decided not needed.
