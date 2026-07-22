# Common Design Philosophy

Some interaction patterns recur across tools rather than belonging to any
one of them. Where that's true, they live once as a reusable
composable/component rather than being reimplemented per screen. This doc
catalogs those shared patterns so future work reaches for them first,
instead of rebuilding something that already exists.

**Currently in use by**: YT Shadowing only (`YtShadowingPlayerScreen.vue` and
`WordInfoPopup.vue`). Recorder Loop and Robot Shadowing predate this work and
are built around their own hands-free, countdown-driven record→playback
cycles (`recorderLoopEngine.js`, `robotShadowingEngine.js`) rather than a
manually-toggled Record/Shadow pair - that's a deliberate difference in
those tools' design (see their own spec docs), not a gap to backfill. Any
*new* tool/screen that needs manual record/playback controls, a play/pause
toggle, or clickable text should reach for the pieces below rather than
writing a new bespoke version.

## Play / Record / Shadow buttons

- **`PlayPauseIcon.vue`** - the triangle/pause-bars icon, `playing: Boolean`
  prop. A host that never needs to pause (see below) just never sets it,
  and only ever gets the triangle.
- **`useRecordShadow.js`** (composable) + **`RecordShadowButtons.vue`**
  (component) - the Record+Shadow button pair: owns the mic engine, the
  Record toggle (start/stop, auto-plays your recording back on stop), and
  the `R`/`S`/`L` and `S`/`R`/`L`/`P` state-code labels for both buttons.
  `RecordShadowButtons.vue` takes a `compact` prop for tighter layouts (the
  word popup) versus the main screen's full-size row - same icons/labels
  either way, just smaller.
  - What a single Shadow *pass* actually does - what "the original" is,
    what order play/record/listen happens in - stays owned by each call
    site, built from the pieces `useRecordShadow` exposes
    (`micEngine`/`micState`/`isShadowing`/`consumeWantsDouble`). That's
    deliberate: YT Shadowing's video-segment pass (record first, replay
    computed backward from the recording's real length afterward) and the
    word popup's fixed-length clip (play first, since its length is already
    known) are genuinely different sequences - forcing one shape onto both
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
  dictionary popup (`WordInfoPopup.vue`, word audio auto-plays on open);
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

## Modal popups

A modal (currently just `WordInfoPopup.vue`) isn't done just by rendering
an overlay on top - two things are easy to miss and both caused real bugs
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

## File map

- `src/composables/useShiftOrLongPress.js`
- `src/composables/useRecordShadow.js`
- `src/components/RecordShadowButtons.vue`
- `src/components/PlayPauseIcon.vue`
- `src/engine/micRecorderEngine.js` (owned internally by `useRecordShadow`)
- `src/engine/externalDictionarySites.js`
- `src/components/WordInfoPopup.vue` and `src/screens/YtShadowingPlayerScreen.vue`
  (the two current consumers of all of the above)
