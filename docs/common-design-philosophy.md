# Common Design Philosophy

Some interaction patterns recur across tools rather than belonging to any
one of them. Where that's true, they live once as a reusable
composable/component rather than being reimplemented per screen. This doc
catalogs those shared patterns so future work reaches for them first,
instead of rebuilding something that already exists.

**Currently in use by**: `YtShadowingPlayerScreen.vue`, `DictionaryPopup.vue`
(a single global instance owned by `App.vue` - see "Modal popups" below -
reachable from anywhere, not just YT Shadowing), and Flashcards' three
session screens (Learn/Review/Practice, via `useFlashcardFaceAudio.js` -
two independent instances per screen, one for the front's text and one for
the back's, plain TTS instead of a real pronunciation clip - see
`flashcards-spec.md`). Recorder Loop and Robot Shadowing predate this work
and are built around their own hands-free, countdown-driven
record→playback cycles (`recorderLoopEngine.js`, `robotShadowingEngine.js`)
rather than a manually-toggled Record/Shadow pair - that's a deliberate
difference in those tools' design (see their own spec docs), not a gap to
backfill. Any *new* tool/screen that needs manual record/playback controls,
a play/pause toggle, or clickable text should reach for the pieces below
rather than writing a new bespoke version.

## Play / Record / Shadow buttons

- **`PlayPauseIcon.vue`** - the triangle/pause-bars icon, `playing: Boolean`
  prop. A host that never needs to pause (see below) just never sets it,
  and only ever gets the triangle.
- **`useRecordShadow.js`** (composable) + **`RecordShadowButtons.vue`**
  (component) - the Record+Shadow button pair: owns the mic engine, the
  Record toggle (start/stop, auto-plays your recording back on stop), and
  the `R`/`S`/`L` and `S`/`R`/`L`/`P` state-code labels for both buttons.
  `RecordShadowButtons.vue` takes a `compact` prop for tighter layouts (the
  dictionary popup) versus the main screen's full-size row - same
  icons/labels either way, just smaller.
  - What a single Shadow *pass* actually does - what "the original" is,
    what order play/record/listen happens in - stays owned by each call
    site, built from the pieces `useRecordShadow` exposes
    (`micEngine`/`micState`/`isShadowing`/`consumeWantsDouble`). That's
    deliberate: YT Shadowing's video-segment pass (record first, replay
    computed backward from the recording's real length afterward) and the
    dictionary popup's fixed-length clip (play first, since its length is
    already known) are genuinely different sequences - forcing one shape onto both
    would've made one of them worse. Only the plumbing that's actually
    identical is shared.
- **`useShiftOrLongPress.js`** (composable) - the generic gesture
  underneath both Shadow's double-pass and Play's temporary slow-speed
  playback (see below): a shift-click (desktop) or a ~500ms long-press
  (touch, no shift key available) are treated as equivalent "alternate
  behavior" requests. Exposes `handlePointerDown(guard?)` /
  `handlePointerUp` / `handlePointerCancel` (wire to the button's own
  pointer events) and `consume(event)` (reads and resets whether the press
  that's about to act asked for the alternate behavior). `useRecordShadow`
  builds its double-pass detection on top of this rather than keeping its
  own copy.

## Clickable (and shift-clickable) words and buttons

The recurring convention: a plain click does the primary/expected thing;
shift-click or a long-press does a secondary thing, without an extra
button to discover it. Currently used in:

- **Transcript words** (YT Shadowing): click pauses the video and opens the
  dictionary popup (`DictionaryPopup.vue`, word audio auto-plays on open);
  shift-click skips the popup entirely and opens Cambridge Dictionary
  directly in a small centered window.
- **Transcript lines** (YT Shadowing): click seeks to and plays from that
  line; shift-click jumps straight into capture/loop mode for just that one
  line (bypassing the Yes/No review the press-and-hold capture path shows).
- **The popup's play buttons** (word header + each phonetic entry): click
  plays at normal speed; shift-click/long-press plays that same clip at
  0.5x - one-shot, no state to restore afterward.
- **The Shadow button itself**: click runs one play/record/listen pass;
  shift-click/long-press runs a second pass immediately after the first.
- **The video's Play button**: click plays/pauses normally; shift-click/
  long-press (only when starting from paused) temporarily drops playback to
  0.5x for that play, restoring whatever speed was set before the moment
  the video next pauses, by any means.

Shared building blocks behind these: `useShiftOrLongPress.js` (the gesture
itself) and **`externalDictionarySites.js`** (`openCenteredWindow`/
`openReferenceSite` - the small-centered-popup-window opener used by both
the shift-click-a-word path and the popup's own "Ca"/"Wr" reference-site
buttons, so there's one implementation of "open this word in an external
dictionary," not two).

Recorder Loop and Robot Shadowing have no clickable text at all today -
neither has a transcript or phrase list on screen (Robot Shadowing
deliberately keeps its phrase hidden, by design, as part of the
listen-and-repeat exercise), so there's nothing to click yet in either.

Flashcards' front/back words are also clickable (same dictionary-popup
destination as transcript words), but deliberately **without** a
shift-click alternate - plain click only, see `flashcards-spec.md`.

## Modal popups

A modal (currently just `DictionaryPopup.vue`) isn't done just by rendering
an overlay on top - a few things are easy to miss and have caused real bugs
the first time around:

- **The rest of the screen must be genuinely unreachable, not just visually
  covered.** A full-screen backdrop blocks pointer clicks by DOM stacking
  alone, but it does nothing about keyboard focus/Tab navigation reaching
  a control hidden behind it, and nothing at all about global
  `window.addEventListener('keydown', ...)` shortcuts the host screen owns
  independently of the DOM tree. The host wraps everything *except* the
  modal in a container bound to `:inert="isModalOpen || null"` (the
  `|| null` matters - binding the bare boolean can leave a stringified
  `inert="false"` attribute behind, which HTML still treats as present/true
  regardless of the string value) and adds an early `if (isModalOpen) return`
  at the top of its keydown handler, since the modal's own Escape-to-close
  listener is separate and doesn't need the host's shortcuts silenced for it
  to work.
- **Closing it must stop, not just hide, anything it started.** If a modal
  can kick off something long-running of its own (recording, playback), the
  same object/engine instance usually persists across open/close cycles
  (the component doesn't unmount, only a `visible` flag toggles) - so
  hiding it without also tearing down that session leaves it running
  invisibly, and reopening the modal for something else doesn't help,
  since it's the same stuck instance underneath. Closing must both stop the
  underlying engine *and* flip an internal flag any in-flight async
  sequence checks between steps - engine `destroy()` alone doesn't cover a
  step that hasn't started yet (e.g. still mid-playback before a recording
  would even begin).
- **A modal usable from *any* screen needs a single global owner, not one
  per screen.** `DictionaryPopup.vue` started out owned locally by
  `YtShadowingPlayerScreen.vue` (the only place that could open it); once it
  needed to open from the Home menu and a global keyboard shortcut too, it
  moved up to a single instance mounted in `App.vue`, sitting alongside
  whichever screen is currently active rather than duplicated per-screen.
  Screens that can trigger it now do so by emitting an event upward (e.g.
  `emit('show-word', word)`) rather than holding their own `ref` to it, and
  read its open/closed state back down via a prop (e.g. `dictionaryOpen`)
  for their own `:inert`/keydown-guard logic - the same two rules above
  still apply, they just now span two components instead of living in one.
  Worth reaching for this same shape for any *future* modal that isn't
  provably confined to a single screen, rather than discovering the need to
  hoist it later.

## Toast notifications

For a brief, non-critical message - a network error, or an FYI that
doesn't need acknowledgment - a **toast** (`useToast.js`'s `showToast()` +
`ToastHost.vue`, mounted once globally in `App.vue`, same shape as
`DictionaryPopup.vue` above) is usually the right call: a small dark pill
near the bottom of the screen that fades in, holds for a few seconds, then
fades out on its own, no tap-to-dismiss needed. `showToast(message,
{type: 'error'})` is callable from anywhere - any component or plain `.js`
module - with no ref/prop-drilling needed, since it's backed by one shared
reactive queue rather than a per-screen instance.

- **What it's for**: something transient happened (an action just failed
  or succeeded) that the user should notice but doesn't need to keep
  looking at - a failed network request being the most common case (e.g. a
  mutating Flashcards call failing while offline), but equally fine for a
  small non-error FYI.
- **What it's not for**: an error that leaves the screen with nothing
  useful to show (e.g. the initial data load for a whole screen failed) -
  that still deserves a permanent, visible explanation instead of
  something that quietly disappears in a few seconds. Keep using a plain
  persistent `error-message` for that case.
- Not (yet) a wholesale replacement for every screen's existing
  `error-message` paragraph - adopted incrementally, starting with
  Flashcards' action failures, not retrofitted everywhere at once.

## Online shared storage

Some data is small, changes over time, and is worth keeping in sync across
devices - a short list, a handful of settings, "where you left off." For
that specific shape of data, a small Cloudflare Worker + KV backend (see
`cloudflare-worker/`) gives it one shared home every device reads and
writes the same copy of, instead of each device quietly keeping its own
local copy that never agrees with the others.

- **What it's for**: small-ish, frequently-changing, worth-keeping data that
  should look the same everywhere you use the app.
- **What it's not for**: anything sizeable (real files, large collections)
  or essentially static (rarely changes, or is fine being device-specific) -
  that belongs in proper server-side storage, or in local/on-device
  storage, not here. This is a thin sync layer for small state, not a
  general-purpose database.
- **Access model**: no interactive sign-in (see `docs/setup-cloudflare.md`
  for the reasoning) - a Worker URL, entered manually per device via a
  settings screen and kept in `localStorage`, is the only thing gating
  access. Deliberately not an OAuth flow, since the whole point is
  instant, friction-free use across your own devices, not securing
  something sensitive.
- Reads/writes go through the Worker acting as a thin API in front of KV (a
  plain key-value store) - the client never talks to KV directly.
- **Best-effort by design**, same convention as `nativeExpServerClient.js`:
  every read/write function degrades to doing nothing (an empty or
  unchanged result) rather than throwing if no server is configured yet or
  the request fails. This kind of sync should never be required for a
  feature to work, only something that improves it when available.

## Local (per-device) storage

The counterpart to "Online shared storage" above: small settings/state
that's fine being device-specific (not synced) still shouldn't be
scattered as one-off `localStorage.getItem`/`setItem` calls per feature.
`StorageMap` (`src/engine/storageMap.js`) gives that shape of data a
single key→JSON-value client, backed by `native-server` where available
and falling back to `localStorage` only when no server is reachable at
all - see `docs/local-storage.md` for the full design. Not heavily used
yet (one setting so far - the sync server URL, `syncConfig.js`), but the
place to reach for next time rather than writing another bespoke
`localStorage` call.

## File map

- `src/composables/useShiftOrLongPress.js`
- `src/composables/useRecordShadow.js`
- `src/components/RecordShadowButtons.vue`
- `src/components/PlayPauseIcon.vue`
- `src/engine/micRecorderEngine.js` (owned internally by `useRecordShadow`)
- `src/engine/externalDictionarySites.js`
- `src/components/DictionaryPopup.vue` (globally mounted once in `App.vue`),
  `src/screens/YtShadowingPlayerScreen.vue` (opens it via `emit('show-word')`),
  `src/screens/HomeScreen.vue` (opens search mode via `emit('open-dictionary')`)
  - see `docs/dictionary-spec.md` for the full picture
- `src/composables/useToast.js`, `src/components/ToastHost.vue` (globally
  mounted once in `App.vue`)
- `cloudflare-worker/` (the Worker + KV backend), `src/engine/syncConfig.js`
  (the shared Worker-URL setting), `src/screens/SettingsScreen.vue` (where
  it's entered) - see `docs/setup-cloudflare.md` for setup
- `src/engine/storageMap.js` (local, per-device key-value client) - see
  `docs/local-storage.md`
