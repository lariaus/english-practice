# Tool: YT Shadowing

## Goal

Paste any YouTube video URL, get a controllable player (seek, speed,
segment looping), and practice shadowing against it - either the video's
own words (via a captions-derived transcript) or arbitrary segments you
mark yourself. A companion feature lets you click any word in the
transcript to see its definition/pronunciation.

Two screens: **`YtShadowingFormScreen.vue`** (paste a URL, or reopen one of
the last 5 videos from history) → **`YtShadowingPlayerScreen.vue`**
(everything below), matching the app's established two-screen pattern
(same shape as Recorder Loop's duration-picker → session screens).

## Video playback core

`ytShadowingEngine.js` wraps the YouTube IFrame Player API:

- Load a video by URL or bare ID, autoplay once ready.
- **Video control bar**: a black strip flush against the bottom of the
  video (rectangular corners throughout - no rounding anywhere in this
  block), visually reading as one attached unit rather than another row of
  app-style buttons. Contents, left to right: current/total time
  (`0:42 / 3:15`), a thin tap-to-seek scrub line spanning the full width of
  the bar above this row, then seek ±5 / Play-Pause / seek ±5 grouped in the
  center, then a compact speed pill on the right.
  - Play/Pause is a small circle (same icon-swap as `PlayPauseIcon.vue`
    elsewhere - triangle when paused, two bars when playing). Shift-click or
    a long-press (only when starting from paused) temporarily drops speed to
    0.5x for that play - a one-off slowdown, not a persistent change:
    whenever the video next pauses, by any means, the speed from before that
    shift/long-press play is restored.
  - Seek ±5s buttons show just `-5`/`+5` (no unit suffix), sized to match
    the compact bar.
  - The speed pill (`1.00x`) is collapsed by default; clicking it opens a
    small popover above the bar containing the original −/bar/+ speed
    control (0.5x-2x in 0.05 steps, bar centered on 1.0x since 0.5-1.0 and
    1.0-2.0 are differently-sized ranges). Clicking the pill again, clicking
    anywhere outside the popover, or pressing `Escape` closes it.
  - **CC** (subtitle toggle) sits at the right end of this same bar, past
    the speed pill.
  - Below a thin divider line, the same black bar continues with a second
    row: Record, Shadow, Auto Shadow, and Capture (in that order), left-
    aligned rather than stretched to fill the row. These keep their normal
    app-wide button look (grey pill, colored accent/red for active states)
    rather than matching the white-on-black seek/speed/CC styling above the
    divider - the line is what separates "video playback controls" from
    "practice tools," not a color change. All four use the same `compact`
    sizing (smaller padding/font/icon, no stretch) that `RecordShadowButtons.vue`
    already had for the word popup - reused here rather than inventing a new
    size tier.
- Keyboard: Space (play/pause), ←/→ (seek ±5s), Shift+←/→ (speed ±0.05).
- A segment "loop" primitive (seek to start, play, poll `getCurrentTime()`
  since the IFrame API has no native "stop at time X" event, jump back to
  start on reaching end) underlies all the capture/shadow features below.

## Capture / loop mode

The Capture button lives in the practice-tools row (see "Video playback
core" above) as the last of the four buttons there, labeled `Cap` (idle and
mid-hold - the mid-hold state is shown via the existing red/pulsing style,
not different text) and `End` (once a range is committed and looping -
shown with a red background, unlike Record/Shadow's accent-gradient
"active" color, so it's unambiguous that pressing it *exits* rather than
continues something). Icon is a small `[ ]` in/out bracket-marker pair.

Three ways to mark a start/end range and start looping it:

1. **Press-and-hold** the Capture button; release to mark the range, which
   shows a "Validate capture of X - Y? Yes/No" overlay before committing.
   Pressing always (re)starts playback if the video was paused - holding on
   a frozen frame would capture nothing.
2. **Select transcript text** (once subtitles are on) spanning one or more
   lines, then click "Capture selection" - captures the smallest span of
   full caption lines containing the selection and jumps straight into
   looping (no Yes/No review, since selecting text was already the
   deliberate choice).
3. **Shift+click** a single transcript line's seek icon - captures just
   that one line, same instant-commit behavior as #2.

All three clamp a cue's end time to the *next* cue's start when needed,
since auto-generated captions routinely overlap (a cue's `duration` is how
long it stays on screen, not the actual speech length).

Once looping, "End" leaves the loop and keeps playing forward from the end
of the range. Keyboard: `C` mirrors the hold/release; `C` (a plain press)
while looping ends capture. Seek/speed controls stay live during a loop
(seeking past the end just snaps back to the start on the next poll).

## Recording & shadowing

Three related but distinct actions, sharing one mic engine
(`micRecorderEngine.js`) so they never run concurrently. Button text is kept
to short state codes rather than full words:

- **Record** (`R`/`S`/`L`): a plain toggle - click/`R` key to start recording
  your voice, click again to stop, which auto-plays your recording back
  immediately. Label reflects `micState.phase` while a Record-owned session
  is active: `R` idle, `S` (speak) while recording, `L` (listen) while your
  recording plays back. Nothing more.
- **Shadow** - icon is two overlapping person silhouettes (a solid figure in
  front, a fainter offset one behind it in the record-red color, standing in
  for "shadow"). Label is always one of `S`/`R`/`L`/`P` (idle / recording /
  listening-to-your-own-playback / hearing-the-original), same codes and
  same meaning whether in or out of capture mode, since both flows pass
  through the same recording/playback/rehear stages:
  - *In capture mode*: one cycle of play the captured range once → beep →
    record your voice (range length, speed-adjusted, +1s) → beep → play
    your recording back → beep. Shift-click/Shift+`S`/long-press runs a
    second cycle immediately after the first, same double-pass convention
    as the outside-capture and word-popup Shadow flows below (previously
    this always ran twice unconditionally - unified for consistency). Stays
    paused at the end, still in capture mode.
  - *Outside capture mode*: a manual record-and-rehear toggle - press to
    record, press again to stop, hear your own recording, then the video
    rewinds by `(your recording's real duration × current speed) + 0.25s`
    and replays that original segment, then resumes whatever play/pause
    state was active before you started. `S` key (pressed twice) mirrors
    this.
    - **Double pass**: shift-click, Shift+`S`, or a ~500ms long-press
      (touch) on the *first* press requests a second speak/listen/replay
      pass immediately after the first, back-to-back, on the same segment -
      only that first press is checked for shift/long-press, every press
      after is a plain stop/no-op like normal. The second pass records for
      a fixed duration (no manual stop needed) equal to how long the first
      pass's replay took, then plays it back, then replays that exact same
      original segment again. You can still press Shadow/`S` to cut the
      second pass's recording short, same as the first - doing so does not
      shrink the replay, which stays pinned to the first pass's timing so
      both passes rehear the identical segment.
- **Auto Shadow** (`Auto`, separate button, disabled during capture mode):
  same shadow icon, but its label never changes text - only the active/
  inactive styling toggles. The autonomous version of Shadow's old
  outside-capture behavior - advances automatically through consecutive
  segments, 2 cycles per segment (via the same `runShadowCycles` as
  everything else here), continuing until there's nothing left to shadow or
  it's stopped early by clicking again. Prefers subtitle-cue lines over
  fixed windows:
  - On start, silently ensures subtitles are fetched (same fetch-once
    `subtitleCues` data the CC button uses) *without* turning on the
    transcript/overlay - so the first-ever use of Auto Shadow (or CC) in a
    session may pause briefly while it checks.
  - If cues are available: starts at whichever cue covers (or comes right
    after) the current position, plays/records/plays-back each cue's own
    span (record length = that cue's own clamped length, speed-adjusted +
    1s, not a fixed duration), then advances cue by cue. Stops once it runs
    out of cues, even if video content continues past the last one.
  - If no cues are available at all (NativeExpServer isn't running, or this
    video has no captions) - or subtitles were never turned on and none
    could be found - falls back to the original fixed-window behavior:
    consecutive 4s windows starting from the current position, continuing
    until the video ends.

Record/Shadow/Auto Shadow, Capture, and the transport controls all disable
each other appropriately while one is active, via a handful of shared
`computed` flags.

## Subtitles & transcript sidebar

Toggled on demand via the **CC** button (not automatic - only fetched once,
lazily, on first toggle-on; toggling off/on again never re-fetches):

- A one-line "currently spoken" caption overlaid on the video.
- A scrollable transcript list alongside/below the video (side-by-side on
  wide screens, stacked on narrow ones), each line clickable to seek there,
  the active line highlighted and lazily auto-scrolled into view (only
  moves once the active line nears the edge, landing at the top rather
  than recentering every line - like Language Reactor's sidebar).
- Clicking a word (not just a line) opens the dictionary popup (below);
  shift-clicking a word instead opens Cambridge Dictionary directly. Both
  are disabled (dimmed, `cursor: not-allowed`) while Record, Shadow, or Auto
  Shadow is active - opening the popup mid-session would fight with
  whatever they're doing with the mic/video.

Subtitles are fetched through **NativeExpServer** (see its own section
below) - entirely optional; if it's not running, the CC button just does
nothing visible.

## Dictionary lookup popup

Clicking a transcript word pauses the video and opens `WordInfoPopup.vue`,
showing:

While open, it's fully modal: the rest of the screen (buttons, keyboard
shortcuts like Space/arrows/`s`/`r`/`c`) is `inert` and unreachable until it
closes - clicking/focusing/pressing keys behind it does nothing. Closing it
(X button, backdrop click, or Escape) immediately stops and resets any
Record/Shadow session that was running inside the popup itself (mic
recording, beeps, playback) - nothing keeps running in the background after
it's gone.

- The word, phonetic transcription(s) labeled by dialect (`US`/`UK`/`AU`,
  or `??` when undetectable), US always listed first when available.
- Up to 2 definitions per part of speech, with examples/synonyms.
- A Wiktionary attribution line (required by the data's CC BY-SA license).
- A play button next to the word, and one per phonetic entry with audio -
  same triangle icon as the video's Play/Pause (`PlayPauseIcon.vue`), but
  these never show the pause variant or toggle anything: a word clip is
  short/one-shot, unlike a video, so there's nothing meaningful to pause.
  Shift-click or a long-press plays that one clip at 0.5x instead of normal
  speed - unlike the video's version, this needs no "restore afterward"
  step, since each click is an independent one-shot play with nothing
  persistent to revert.
- **Record and Shadow buttons** (via the shared `useRecordShadow.js` +
  `RecordShadowButtons.vue` also used by YT Shadowing's own controls - same
  icons, labels, and mic engine, identical R/S/L and S/R/L/P codes):
  - *Record*: plain toggle, auto-plays your recording back on stop.
  - *Shadow*: one pass of play-the-word → record → listen-to-yourself. The
    word's own clip is played first, so (unlike the video's Shadow, which
    has to size its replay from your recording afterward) its length is
    already known - that measured length + 0.25s becomes the recording
    window, no guessing needed. Shift-click or a long-press (same ~500ms
    gesture as the video's Shadow) runs a second pass immediately after the
    first, replaying the word again rather than reusing the first pass's
    clip.
- Quick links to open the word in Cambridge Dictionary or WordReference,
  each in a small centered popup window.
- `Escape` closes the popup.

This is deliberately **independent of NativeExpServer** - `dictionaryClient.js`
fetches directly from the free, keyless Free Dictionary API
(`api.dictionaryapi.dev`) client-side, with an in-memory cache so repeat
lookups of the same word never re-fetch.

Pronunciation playback (`wordAudioPlayer.js`) tries a real recorded
American-English clip from the dictionary data first, falling back to
Google Translate TTS if none exists for that word - `playWordPronunciationTimed`
is the same fallback chain, awaitable, resolving with the elapsed seconds
once the clip finishes (used by Shadow above).

## History

The last 5 distinct videos watched (title + URL) show on the form screen,
persisted in `localStorage`; re-watching a video moves it to the top
instead of duplicating it.

## Architecture fit

- Screens: `YtShadowingFormScreen.vue`, `YtShadowingPlayerScreen.vue`.
- Engines (plain JS, no Vue): `ytShadowingEngine.js` (player + loop/range
  logic), `micRecorderEngine.js` (record/play/beep primitives),
  `dictionaryClient.js` (fetch+cache word info), `wordAudioPlayer.js`
  (pronunciation fallback chain), `externalDictionarySites.js`
  (centered-window openers), `nativeExpServerClient.js` (talks to the
  companion server), `ytHistory.js` (localStorage history).
- Composables: `useRecordShadow.js` - the Record/Shadow mic-engine plumbing
  (toggle, R/S/L + S/R/L/P labels, shift/long-press double-pass detection)
  shared by every Record+Shadow pair in the app. What one Shadow *pass*
  actually does (what "the original" is, what order play/record/listen
  happens in) stays owned by each call site, built from the pieces this
  exposes - it's genuinely different between YT Shadowing's video segment
  (record first, replay computed afterward) and the word popup's fixed-length
  clip (play first, since its length is already known) - see "Recording &
  shadowing" and "Dictionary lookup popup" above. `useShiftOrLongPress.js` -
  the even more generic "was this press shift-clicked or long-pressed"
  gesture underneath both Shadow's double-pass and Play's temporary
  slow-speed playback; `useRecordShadow.js` builds on top of it rather than
  duplicating its own copy.
- Components: `RecordShadowButtons.vue` (the Record + Shadow buttons
  themselves - same markup/icons wherever it's used, fed by a
  `useRecordShadow.js` instance; takes a `compact` prop for tighter contexts
  like the word popup, versus the main screen's full-size row),
  `PlayPauseIcon.vue` (the shared triangle/pause-bars icon, takes a
  `playing` prop - the video's Play/Pause button uses both icon states, the
  popup's play buttons only ever use the triangle), and `WordInfoPopup.vue`
  (reusable beyond this tool by design - fetch/cache logic and display logic
  both live outside YT Shadowing's own files - so any future part of the app
  can show the same popup, Record/Shadow buttons included).

## NativeExpServer (companion process)

A small, optional local server (`native-exp-server/`, plain Python stdlib
`http.server` + the `youtube-transcript-api` package) that fetches YouTube
captions on the app's behalf, since a browser can't read them directly (no
CORS headers on YouTube's caption endpoint). Exposes `GET /health` and
`GET /subtitles?url=...&lang=en`. The app pings `/health` first and
silently skips subtitle-dependent features if it's not reachable - never
required for the rest of the app to work. See its own `README.md` for setup
and `docs/dictionnary_sources_research.md` for the research behind the
dictionary/pronunciation feature choices.

## Explicitly out of scope / known limitations

- **Word-precise capture from a text selection isn't possible** - caption
  data only has one timestamp per whole line, not per word, so
  selection-based capture always rounds outward to the full line(s)
  touched, never a sub-line span.
- **Dictionary audio coverage is inconsistent** - it's crowdsourced, so not
  every word has a recorded clip, and there's no reliable field indicating
  dialect when a word has no explicitly-US-labeled entry (falls back to
  `??`).
- **NativeExpServer must be started manually** alongside the static server
  - there's no auto-launch, and it only needs to be running for the CC/
  subtitles feature specifically (word lookups work without it).
