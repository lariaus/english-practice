# Tool: Robot Shadowing

## Goal

A shadowing practice loop: hear a phrase spoken aloud by the browser's
built-in TTS, repeat it back within a time window, then hear your own
recording played back — continuously, hands-free, cycling through a random
phrase database until stopped. (Contrast with Recorder Loop, which just
replays your own free-form speech with no prompt.)

## Core behavior

Continuous automatic loop:

1. Pick a random phrase from the phrase database (avoid repeating the
   immediately previous phrase).
2. TTS speaks the phrase aloud.
3. RECORD for (actual TTS speaking time + 1) seconds — repeat the phrase.
4. If "Repeat model" is on, TTS speaks the same phrase again (the reference)
   right before playback.
5. PLAYBACK of what was just recorded.
6. If "Repeat" is set above 1, repeat steps 2-5 for the *same* phrase that
   many times before moving on. Only then does step 1 pick a new phrase.

A short beep marks each transition (before TTS, before recording, before
playback) — same pattern as Recorder Loop, so the cycle can be followed
without watching the screen. Unlike Recorder Loop, there's no visible
countdown ring/timer — just a phase label (Listen… / Your turn / Playing
back). A ticking countdown here felt stressful rather than useful, since the
response window itself is already variable per phrase.

## Session options (on the tool screen, before Start)

- **Voice**: dropdown, "Google TTS API" (default) or "Default Engine
  (Samantha)" — see TTS section below.
- **Database**: dropdown, "Easy" (default) / "Medium" / "Hard" — which
  phrase-difficulty tier to draw from (see Phrase database below).
- **Repeat**: buttons 1 / 2 / 3 (default 1) — how many times to redo the
  same phrase before a new random one is picked.
- **Repeat model**: on/off button, default off — replays the reference
  phrase once more right before your own recording plays back, so you can
  compare them back-to-back.
- All four are locked (disabled) once a session is running.

## No duration picker

Unlike Recorder Loop, this tool has no duration selection step — tapping it
on the Home screen goes straight to Start/Stop. The response window isn't a
fixed user-chosen value; it's measured per phrase from how long the TTS
actually took to say it (start → `end` event), plus a 1 second buffer. A
short phrase gives a short response window, a long phrase gives more time,
matching what was just heard. A fixed generic duration (as used by Recorder
Loop's free-form talking) doesn't make sense here since phrase length
varies.

## Phrase database

- Three difficulty tiers, each a plain JSON file bundled into the app —
  `src/data/db-shadowing-easy.json`, `-medium.json`, `-hard.json` — a flat
  array of strings each. No backend, no build step needed to edit.
- 1,000 sentences per tier, genuinely written (not templated/mechanically
  generated) to actually make sense, at these word-count/complexity targets:
  - **Easy**: 4-7 words, beginner vocabulary, simple present/past, no
    subordinate clauses.
  - **Medium**: 8-12 words, still simple grammar, more descriptive
    (adjectives, time/place phrases).
  - **Hard**: 14-22 words, subordinate clauses and connectors ("although",
    "because", "since", etc.), natural fluent-level phrasing.
- Selected via the "Database" dropdown (default Easy); picked at random each
  cycle within the chosen tier, avoiding an immediate repeat of the same
  phrase twice in a row.
- Not shown on screen while it plays or during recording — pure
  listen-and-repeat, closer to real shadowing than reading text aloud. Open
  to revisit if that proves too hard in practice.

## TTS

Two selectable backends, both wrapped behind a uniform `TTSEngine` class
(`src/engine/ttsEngine.js`) so the rest of the app doesn't care which is
active:

- **`google-tts-api`** (default) — the unofficial Google Translate TTS
  endpoint (`translate.google.com/translate_tts`), no API key, no signup.
  Free and noticeably better quality than the OS voices. Two gotchas this
  required working around:
  - The endpoint 404s cross-origin requests carrying a `Referer` header
    (hotlink protection) — fixed with a page-wide `<meta name="referrer"
    content="no-referrer">` in `index.html` (the per-element
    `referrerPolicy` attribute wasn't reliably honored by Safari).
  - No CORS headers, so the audio can't be fetched/decoded via
    `AudioContext` — it plays through a plain, single reused `<audio>`
    element instead (primed within the Start click, like the beep
    `AudioContext`, to dodge autoplay blocking).
  - Same phrase text spoken twice in a row (a shadowing repeat, or "Repeat
    model") replays the already-loaded audio instead of re-fetching, so
    e.g. repeat=2 + Repeat model = 4 speaks but only 1 network request.
  - Unofficial/undocumented — could break or get rate-limited without
    notice; low risk for personal, low-volume use.
- **`default-engine:Samantha`** — the browser's built-in Web Speech API
  (`speechSynthesis`), zero network dependency, always available as a
  fallback. Rated noticeably lower quality by ear, but reliable offline.
- On iOS Safari, voice *selection within* the Web Speech API is unreliable
  (a documented WebKit bug — different named voices don't actually change
  the output) — irrelevant now since Google TTS is the default and doesn't
  hit that bug.

## Architecture fit

- Second tool on the Home screen, alongside Recorder Loop.
- New framework-agnostic engine (`src/engine/robotShadowingEngine.js`),
  reusing the phase/beep/Wake Lock/`MediaRecorder` patterns already built
  for `recorderLoopEngine.js`, with a thin Vue view layer on top — same
  engine/view split as Recorder Loop.
- TTS backend selection lives in its own `TTSEngine` wrapper
  (`src/engine/ttsEngine.js`), decoupled from `robotShadowingEngine.js`, so
  new backends can be added later without touching the main engine.

## Explicitly out of scope for now

- Phrase categorization beyond the three difficulty tiers, or curation
  tooling — just three flat lists for now.
- Showing/hiding phrase text as a user-configurable toggle — currently
  hard-coded to hidden.
