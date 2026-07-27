# Cross-device sync - full pipeline

The complete story of how a piece of app state (today: the "last 5 videos"
history) gets from one device to another: why it exists, every step of
setting it up, how data actually flows end-to-end when the app is used, and
the code underneath each piece. For the general "when should a feature use
this at all" guidance, see `docs/common-design-philosophy.md`'s "Online
shared storage" section. For copy-pasteable setup commands with no
narrative, see `docs/setup-cloudflare.md`. This doc is deliberately
detailed and literal - written to be read by a human or an AI agent picking
up this codebase cold, without needing to re-derive the design from the
code alone.

## Why this exists

Some app state needs to be the *same* across every device the app runs on -
not siloed per-browser the way plain `localStorage` would leave it. Rather
than a full account system, this is the minimum viable version: one small
Cloudflare Worker acting as a thin HTTP API in front of one KV (key-value)
store, with no interactive sign-in anywhere in the loop - see "Security
model" below for why that's an acceptable trade-off here specifically.

This is **not** a general-purpose backend. It's a thin sync layer for
small, frequently-changing, worth-keeping state - see the "what it's for /
not for" split in `common-design-philosophy.md`.

## The pipeline, end to end

**Setup (one-time, done by a human):**

1. Create a free Cloudflare account.
2. Install/authenticate `wrangler` (the CLI) on the machine doing the
   deploying.
3. Create a KV namespace - this is the empty "database," not yet holding
   any data.
4. Write and deploy the Worker code (`cloudflare-worker/src/index.js`) -
   Cloudflare hosts it and gives back a URL like
   `https://english-practice-sync.<subdomain>.workers.dev`.
5. On **each device** that should sync, open the app's Settings screen and
   paste in that URL. It's saved to that device's own `localStorage` - the
   URL itself is never in source control, so this step has to happen again
   on every new device (see "Where the Worker's address lives," below).

Exact commands for steps 1-4 live in `docs/setup-cloudflare.md` - this doc
explains what each step accomplishes, that one just gives you the commands
to run.

**Runtime (every time the app is actually used):**

1. **A video starts.** Once the YouTube player reports its metadata,
   `YtShadowingPlayerScreen.vue` calls `addToHistory()` with the title,
   author, and current duration. This travels over `fetch` to
   `POST https://<worker-url>/history`.
2. **The Worker receives the request**, reads the current `history` value
   out of KV, merges the new entry to the front (deduping by video ID,
   trimming to 5 - see "Data model" below for exactly how), writes the
   merged array back to KV, and returns it in the response.
3. **The device updates its own view** from that response - no separate
   re-fetch needed.
4. **A second device** (or the same device, later) opens the History list:
   `YtShadowingFormScreen.vue` calls `loadHistory()`, a plain
   `GET /history`, and gets back the same array - because it's reading the
   same KV entry, not a per-browser copy.
5. **The watch session ends** (Back button, or the page actually
   closing/reloading/backgrounding) - a second write updates just the
   `currentPosition` field, via a mechanism that survives the page
   unloading (`sendBeacon`, not a plain `fetch` - see "Client-side pieces"
   below for why that distinction matters).
6. **Opening that video again**, on any device, checks the same history
   data for a saved position and silently resumes there if one exists.

That's the whole loop: every device talks to the same Worker, which is the
only thing that talks to KV, which is the one shared source of truth.

## Where the Worker's address lives (and why)

The Worker's URL is **never** written into source, `package.json`, or any
committed file. It only ever exists in:

1. The terminal output of `wrangler deploy`, once.
2. Each device's own `localStorage`, entered by hand via
   `src/screens/SettingsScreen.vue` (persisted through
   `src/engine/syncConfig.js`, key `sync-server-url`).

There is **no authentication at all** on the Worker itself - no API key, no
token, no header check. The URL is the only thing gating access; anyone who
has it can read/write the history. Deliberate, not an oversight:

- The data involved (a personal video list) isn't sensitive - the concern
  was never confidentiality.
- The concern that *was* real - "could someone abusing a leaked URL cost me
  money" - doesn't apply on Cloudflare's free tier: exceeding the daily
  request/read/write quotas returns errors (HTTP 429/1027); it does not
  bill for overage. Overage billing only becomes possible if this account
  is later, deliberately upgraded to the paid Workers plan.
- A Bearer-token version was actually built first, then explicitly
  reverted in favor of this simpler model once both of the above were
  weighed - worth knowing if the reasoning ever needs re-litigating.

## Data model

One KV entry (key: `history`, see `HISTORY_KEY` in `index.js`), a JSON
array, newest-first, capped at 5 entries (`HISTORY_LIMIT`). Each entry:

```jsonc
{
  "videoId": "abc123",           // YouTube video ID - the dedupe key
  "url": "https://youtu.be/...", // full URL, used to reload the video
  "title": "...",                // from the YouTube IFrame API's getVideoData()
  "author": "...",               // channel name - same source, undocumented API (see below)
  "duration": 754,               // seconds, always a fresh/current value when sent
  "currentPosition": 231         // seconds - the one *protected* field, see below
}
```

- `videoId` / `url` / `title` are required on every `POST` (the Worker
  400s without them).
- `author` / `duration` are optional; missing `author` becomes `null`,
  missing/non-numeric `duration` becomes `0`. Both are always sent as
  fresh, current values by the client - no merge logic needed for them.
- `currentPosition` is **protected**: a `POST` that omits it (or sends a
  non-number) leaves whatever was already stored untouched, defaulting to
  `0` only if this is a brand-new entry. A `POST` that sends a real number
  actually updates it. This is the one piece of merge logic in the whole
  Worker (`withEntryAddedToFront()`) - everything else is a flat
  overwrite-by-`videoId`.

Why `author` is undocumented, and why that's an accepted risk: the YouTube
IFrame Player API's `getVideoData()` method returns `{ video_id, title,
author, ... }`, but Google has never listed it in the official IFrame API
reference, and it's been reported removed from docs at some point even
though it still works today. It could stop returning `author` (or stop
existing) without notice - accepted explicitly, since there's no officially
supported alternative short of the YouTube Data API v3 (which needs its own
key and quota).

## Worker code (`cloudflare-worker/src/index.js`)

```js
export default {
  async fetch(request, env) {
    if (request.method === 'OPTIONS') return new Response(null, { headers: CORS_HEADERS })

    const url = new URL(request.url)

    if (url.pathname === '/history' && request.method === 'GET') {
      const history = (await env.HISTORY.get(HISTORY_KEY, 'json')) ?? []
      return jsonResponse(history)
    }

    if (url.pathname === '/history' && request.method === 'POST') {
      const body = await request.json().catch(() => null)
      if (!body?.videoId || !body?.url || !body?.title) {
        return jsonResponse({ error: 'videoId, url, and title are required' }, 400)
      }
      const current = (await env.HISTORY.get(HISTORY_KEY, 'json')) ?? []
      const updated = withEntryAddedToFront(current, { /* ...entry fields... */ })
      await env.HISTORY.put(HISTORY_KEY, JSON.stringify(updated))
      return jsonResponse(updated)  // returns the computed value, doesn't re-read
    }

    return jsonResponse({ error: 'Not found' }, 404)
  },
}
```

Notable details:

- **CORS**: `Access-Control-Allow-Origin: '*'` on every response, plus an
  `OPTIONS` short-circuit for preflight requests. Wide open deliberately -
  there's no fixed origin to restrict to (the app might be self-hosted at
  various URLs), and the real access control (such as it is) is "do you
  know the URL," not CORS.
- **No read-after-write**: the `POST` handler returns the array it just
  computed in memory, rather than re-reading from KV after the `put()`. KV
  is only *eventually* consistent (writes can take up to 60s to propagate
  globally) - reading immediately after writing could race and return
  stale data, so the handler just trusts its own just-computed value
  instead.
- **`withEntryAddedToFront(history, entry)`**: finds any existing entry with
  the same `videoId`, filters it out, then prepends a merged entry (real
  fields from the incoming `entry`, except `currentPosition`, which falls
  back to the existing entry's value - or `0` - if the incoming one isn't a
  number), and slices to `HISTORY_LIMIT`.

## Client-side pieces

### `src/engine/syncConfig.js`

`getSyncServerUrl()` / `setSyncServerUrl(url)` - thin wrapper around one
`localStorage` key. Every other sync-related module reads the URL through
this, never touches `localStorage` for it directly. Trims trailing slashes
on save so `${serverUrl}/history` never ends up with a double slash.

### `src/screens/SettingsScreen.vue`

Where a human sets the URL. A text input bound to `getSyncServerUrl()`, a
Save button (`setSyncServerUrl` + a transient "Saved" label), and a Test
button that does a live `GET /history` against whatever's currently typed
(not necessarily saved yet) and shows a green "✓ Connected" or red "✗ Could
not reach server" message. Reachable from `HomeScreen.vue` via a gear icon
(top-right corner, mirroring the app-wide `.back-button`'s top-left
position but scoped to this screen only - `style.css`'s shared
`.back-button` rule is untouched).

### `src/engine/ytHistory.js`

The only module that calls the Worker's `/history` endpoints:

- `loadHistory()` - `GET /history`. Returns `[]` on any failure (no server
  configured, network error, non-2xx, bad JSON) - never throws.
- `addToHistory(entry)` - `POST /history` via a normal `fetch`. Used both
  when a video starts (no `currentPosition` in the object) and for the
  in-app-Back exit path (real `currentPosition` included) - the function
  itself doesn't care which; the caller decides what to put in `entry`.
- `sendHistoryBeacon(entry)` - same endpoint, but via
  `navigator.sendBeacon()` instead of `fetch`, for the moment the page is
  actually unloading (a normal `fetch` gets cancelled mid-flight once
  unload starts; `sendBeacon` is specified to still deliver). Sent as a
  plain string, not with an explicit `application/json` content type -
  JSON isn't one of the three CORS-safelisted content types, and
  `sendBeacon` can't perform the preflight a non-safelisted type would
  need for a cross-origin request. A plain string body defaults to
  `text/plain`, which *is* safelisted; the Worker's `request.json()` parses
  the raw body regardless of what content type was actually declared, so
  this works without the Worker needing to know or care.

### `src/screens/YtShadowingPlayerScreen.vue` integration

Three moments this screen talks to history, all in one file:

1. **Video starts** - a `watch(() => state.videoTitle, ...)` fires once the
   YouTube IFrame API reports the video's metadata (asynchronous, after the
   player actually starts). Calls `addToHistory({ videoId, url, title,
   author, duration })` - no `currentPosition`, so any previously-recorded
   position is left alone. Sets a local `historyEntryStarted` flag so the
   exit-time writes below know whether there's actually anything to update
   (if the title/metadata never loaded before the user left, nothing is
   sent at all).
2. **Session ends** - three listeners, all building the same payload via
   `buildHistoryExitEntry()` (title/author/duration/`engine.getCurrentTime()`):
   - `handleBack()` (the in-app Back button - a normal Vue screen swap, not
     a real navigation) calls `addToHistory(...)` (regular `fetch`, not
     awaited - nothing to block on before leaving the screen).
   - `window.addEventListener('pagehide', ...)` - the page actually
     closing, reloading, or navigating away. Calls `sendHistoryBeacon(...)`.
   - `document.addEventListener('visibilitychange', ...)`, checking for
     `document.visibilityState === 'hidden'` - the tab backgrounding
     (switching apps, locking the phone). Also calls `sendHistoryBeacon(...)`.
     This exists specifically because iOS Safari can suspend or kill a
     backgrounded tab outright without ever firing `pagehide` - by also
     saving on backgrounding (which almost always happens before an
     eventual kill), the position is very likely already saved even if the
     final termination itself never runs any JS. Firing more than once
     across a session (background → resume → background again) is
     harmless, since each write is idempotent.
3. **Video (re)starts, resuming position** - `resumeFromSavedPositionIfAny()`,
   called in `onMounted` right after the video loads. Calls `loadHistory()`,
   finds the entry matching this `videoId`, and if its `currentPosition` is
   at least 5 seconds (`RESUME_MIN_POSITION_SECONDS`), calls
   `engine.seekTo(savedPosition)` - silently, no prompt. Applies regardless
   of how the video was opened (History click or a freshly pasted URL) -
   the screen only ever checks "is there a saved position for this
   `videoId`," not how it got here. Only works if the video is still within
   the current top-5 (reuses the existing list endpoint rather than adding
   a dedicated lookup).

## Cost

Effectively $0/month for a single-user app on Cloudflare's free tier - see
`docs/setup-cloudflare.md`'s cost discussion if this ever needs re-checking
against current pricing (things like this should be re-verified against
Cloudflare's own docs rather than trusted from memory, since limits/pricing
do shift over time).

## Extending this pattern for a new use case

1. Add a new KV key (or reuse `history` if it's genuinely the same data) -
   don't put unrelated data inside the existing `history` array.
2. Add new `if (url.pathname === '/whatever' && request.method === ...)`
   branches to the same `fetch` handler - no new Worker/deployment needed
   for a second piece of synced state.
3. If the new data has a "changes at a different cadence than the rest of
   the entry" field (like `currentPosition`), give it the same
   protected-merge treatment `withEntryAddedToFront` gives `currentPosition`
   - look up the existing value, fall back to it when the incoming write
   doesn't include a real one. Don't build a fully generic partial-update
   system for this - keep each field's merge behavior explicit and visible
   in the handler, the same way `currentPosition`'s is today.
4. Add the new fetch calls to a small dedicated client module (mirroring
   `ytHistory.js`), not scattered `fetch()` calls in a screen component -
   keeps every sync-related network call best-effort (never throws, no
   server configured → silently do nothing) in one place per feature.
5. If the new thing needs to survive the page actually closing (not just an
   in-app navigation), remember `fetch` gets cancelled on unload -
   `sendBeacon` (plain-string body, see above) is the pattern, not a race
   against `beforeunload`.
