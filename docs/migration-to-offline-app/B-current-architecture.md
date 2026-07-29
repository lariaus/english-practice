# Current Architecture (snapshot, captured 2026-07-28)

A fixed "before" picture of how the app works at the start of this
migration, from reading the full source tree and all existing `docs/`
files. Not maintained live as the rewrite progresses - see this directory's
`README.md`.

## Frontend

- Vue 3 (`<script setup>`, Composition API) + Vite. Pure client-side SPA,
  no server-rendering. `vite build` outputs a static `dist/` folder.
- Engine/view split throughout: framework-agnostic plain-JS "engines" own
  state machines, timers, audio APIs, network calls; Vue components are a
  thin binding layer. See `docs/app-overview.md`.
- Four tools reachable from a Home screen menu: **Recorder Loop**, **Robot
  Shadowing**, **YT Shadowing**, **Dictionary** (the last is a global popup,
  not a navigated-to screen - see `docs/dictionary-spec.md`).
- No router library, no state management library - a single reactive
  "current screen" ref in `App.vue` (`screen.value = { name, ...params }`).

### File map / rough size (lines), largest first

```
src/screens/YtShadowingPlayerScreen.vue   2306   (video controls, capture/loop,
                                                   subtitles, history, dict trigger)
src/components/DictionaryPopup.vue         502
src/engine/robotShadowingEngine.js         426
src/engine/recorderLoopEngine.js           360
src/engine/ytShadowingEngine.js            305   (YouTube IFrame Player wrapper,
                                                   loop/range/capture logic)
src/engine/micRecorderEngine.js            301   (record/play/beep primitives)
src/engine/ttsEngine.js                    205   (Google TTS / Web Speech wrapper)
src/composables/useRecordShadow.js         186
src/components/RecordShadowButtons.vue     174
src/screens/YtShadowingFormScreen.vue      155
src/screens/SettingsScreen.vue             150
src/screens/HomeScreen.vue                 148
src/screens/RobotShadowingSessionScreen    121
src/screens/RecorderLoopSessionScreen      115
src/engine/dictionaryClient.js              94
src/engine/wordAudioPlayer.js               83
src/engine/nativeExpServerClient.js         68   (client for the Python companion server)
src/engine/ytHistory.js                     72   (client for the Cloudflare Worker)
src/composables/useShiftOrLongPress.js      49
src/engine/syncConfig.js                    28   (localStorage-backed Worker URL setting)
src/screens/DurationPickerScreen.vue        28
src/components/PlayPauseIcon.vue            15
```

`YtShadowingPlayerScreen.vue` is disproportionately large and is where most
of the server-facing behavior (subtitles fetch, history read/write, capture
state) currently lives inline in a screen component rather than an engine -
worth keeping in mind for later steps, though not itself part of this
migration's checklist.

## The two Python processes (this is two things, not one)

The checklist's "the static python server" and "companion exp server" are
genuinely two separate pieces today - worth not conflating:

1. **The static file server** - not custom code at all. It's the literal
   README-documented command:
   ```
   python3 -m http.server 8000 --directory dist
   ```
   Serves the built `dist/` folder, bound to `0.0.0.0` so it's reachable
   from an iPhone on the same Wi-Fi. Nothing to "port" except this
   behavior - there's no existing source file for it.

2. **`native-exp-server/server.py`** - real custom code. Plain stdlib
   `http.server` (`ThreadingHTTPServer`), two endpoints:
   - `GET /health` → `{ "status": "ok", "service": "NativeExpServer" }`
   - `GET /subtitles?url=<youtube-url-or-id>&lang=en` → calls the
     `youtube-transcript-api` Python package (an unofficial scraper of
     YouTube's caption data - no official API, no CORS, which is why the
     browser can't do this itself) and returns a flat list of `{ text,
     start, duration }` cues.
   - Listens on `0.0.0.0:5905` (overridable via `HOST`/`PORT` env vars).
   - No auth - explicitly local/LAN-only, never meant to face the public
     internet.
   - The web app pings `/health` first (`nativeExpServerClient.js`,
     `isNativeExpServerAvailable()`, 1s timeout) and silently skips
     subtitle-dependent features if it's unreachable - never required for
     the rest of the app.
   - `fetchSubtitles()` has a 10s timeout since the Python server itself has
     to round-trip to YouTube.

Both are started manually, side by side, during local dev/testing - no
process manager, no auto-launch.

## Cloudflare sync (already done, step 1 of the checklist)

- `cloudflare-worker/src/index.js` - a Cloudflare Worker in front of one KV
  namespace (binding `HISTORY`).
- One KV key (`history`), one JSON array, newest-first, capped at 5 entries.
  Each entry: `{ videoId, url, title, author, duration, currentPosition }`.
- Two endpoints on the Worker: `GET /history`, `POST /history` (upsert by
  `videoId`, `currentPosition` is the one protected/merge-preserved field -
  see `docs/cross-device-sync.md` for the exact merge logic).
- **No authentication** - the Worker URL itself is the only thing gating
  access, deliberate (see `docs/cross-device-sync.md` "Security model" /
  `docs/setup-cloudflare.md`). CORS wide open (`*`).
- Client side: `src/engine/ytHistory.js` (the only module that calls the
  Worker), reading the Worker's URL via `src/engine/syncConfig.js`.
- Every sync call is best-effort - no server configured, or any failure,
  degrades to a no-op/empty result, never throws. Same convention as
  `nativeExpServerClient.js`.

## `localStorage` footprint today

Grepped the full `src/` tree - **exactly one key is actually read/written**:

- `sync-server-url` (`src/engine/syncConfig.js`) - the Cloudflare Worker's
  URL, entered by hand per device via `SettingsScreen.vue`. Never committed
  to source (see `docs/cross-device-sync.md`).

Everything else that might sound like client-local storage (YT watch
history, positions) already lives in Cloudflare KV, not `localStorage` -
that migration already happened. So "replace all localStorage use cases"
(checklist step 8) is a much smaller surface than it might sound: really
just "where does the bootstrap Worker-URL setting live once there's a local
server" - see the open question in `D-open-questions.md`.

## External network dependencies baked into the client

Relevant to how far "offline" can realistically go per tool (see
`A-vision-and-goals.md` principle #1):

| Host | Used by | Purpose |
|---|---|---|
| `www.youtube.com` | `ytShadowingEngine.js` | YouTube IFrame Player API - the video itself |
| `api.dictionaryapi.dev` | `dictionaryClient.js` | Free Dictionary API - word definitions/phonetics |
| `translate.google.com` | `ttsEngine.js` | Unofficial Google Translate TTS endpoint |
| `dictionary.cambridge.org`, `www.wordreference.com` | `externalDictionarySites.js` | External reference links, opened in a new window - not fetched/embedded |
| (Cloudflare Worker's own URL, user-configured) | `ytHistory.js` | Cross-device history sync |
| `localhost`/LAN, port 5905 | `nativeExpServerClient.js` | The Python companion server |

Recorder Loop is the only tool with zero entries in this table.

## Deploy/hosting model (pre-migration)

- GitHub Actions (`.github/workflows/deploy.yml`) builds (`npm ci && npm run
  build`) and publishes `dist/` to GitHub Pages on every push to `main`.
- Live at `https://lariaus.github.io/english-practice/` (per `README.md`).
- Local device testing (laptop + iPhone on the same Wi-Fi) additionally
  requires a Cloudflare quick tunnel (`cloudflared tunnel --url
  http://localhost:8000`) because iOS Safari only treats `localhost` as a
  secure context automatically - a LAN IP over plain HTTP doesn't qualify,
  and `getUserMedia` (mic access) requires a secure context. This is *why*
  Cloudflare tunneling is already part of the workflow today, separate from
  the Cloudflare Worker/KV sync piece (same vendor, unrelated purpose).
