# Project: Self-Monitoring Loop for Pronunciation Practice

## Context / Goal

I'm a French native speaker (C1 English, fluent, no grammar/vocab issues) working
on reducing my French accent and improving pronunciation clarity for professional
English. My main goals, in priority order:

1. Be clear and understood when speaking English (most important)
2. Have correct pronunciation
3. Sound more American (nice to have, doesn't need to be perfect)

## The Tool: "Self-Monitoring Loop" (not shadowing)

Note: this is deliberately NOT shadowing (which requires repeating a native
reference recording). This is a self-feedback loop: I record my own voice,
then immediately hear it played back to me, so I can catch my own accent
patterns in near-real-time, on a continuous automatic cycle.

### Core behavior

A continuous automatic loop with two alternating states:

1. **RECORD** for N seconds (mic captures my voice)
2. **PLAYBACK** for the same N seconds (immediately plays back what was just recorded)
3. Loop back to step 1, repeat continuously until I stop the session

This should run fully automatically once started — no taps required between
cycles. I want to be able to set it on a table/stand and just talk, with it
cycling record/playback hands-free.

### Modes

Selectable segment duration: **5s / 10s / 15s / 30s / 60s / 90s** (record N
seconds, then play back N seconds, repeat).

### Required behaviors

- Start / Stop controls
- A short audio beep at each transition (before recording starts, before
  playback starts) so the cycle can be followed without looking at the screen
- Clear visual indicator of current state (Recording vs Playing) and a
  countdown/progress indicator for the current segment
- Loop must keep running automatically without user interaction between segments
- Should request microphone permission cleanly on first use

Decided against: a history list of recent segments to replay on demand —
not needed in practice, dropped to keep the tool simple.

## Platform & Hosting

- **Target device:** iPhone 13, running iOS 16.4+ (need to confirm exact
  version, but should already be recent enough — Wake Lock API requires 16.4+)
- **Delivery method:** Safari web app (PWA-style), NOT a native app for now.
  - No server/backend needed — everything runs client-side (mic capture,
    recording, playback all happen in-browser via standard Web APIs)
  - Must be served over **HTTPS** (required for `getUserMedia`/mic access —
    won't work over plain HTTP)
  - **Hosting: GitHub Pages** — free static hosting, HTTPS by default, works
    from anywhere with signal, no server maintenance
  - Add "Add to Home Screen" support so it behaves like a standalone app
    (full-screen, own icon, no Safari chrome) — done, with a generated icon
    set and manifest
  - Offline support via service worker: decided not needed, dropped

## Key technical requirements

1. **Audio recording/playback:** use `getUserMedia` + `MediaRecorder` for
   capture, standard `Audio`/Web Audio playback for replay. Handle Safari's
   audio format quirks (Safari doesn't support all MediaRecorder mimeTypes
   that Chrome does — need to check supported formats on iOS Safari
   specifically, e.g. audio/mp4 vs audio/webm).
2. **Keep screen awake during a session:** use the **Screen Wake Lock API**
   (`navigator.wakeLock.request('screen')`) to prevent the phone from
   auto-locking/dimming while a session is running. Note the known
   limitations:
   - Only prevents *automatic* sleep/dimming — does not override a manual
     press of the side button, and does not survive switching away from the
     Safari tab/app (mic access + timers pause if backgrounded)
   - Re-acquire the wake lock if it's released (e.g. due to visibility
     changes) while a session is still active
3. This is meant to be used standing/propped on a table, foreground, Safari
   open — not as a background/lock-screen tool. That's an accepted
   constraint, not a bug to work around.

## Explicitly out of scope for now

- True shadowing mode (playing native reference audio for me to repeat) —
  may explore later but not part of this build
- Native iOS app via Xcode — considered as a fallback option (free Apple ID
  sideloading works but expires every 7 days without a paid $99/yr Developer
  account) but starting with the Safari/PWA route first
- Any backend, accounts, or data storage beyond local/in-session state
- Automated pronunciation scoring/AI feedback — this is a pure self-listening
  tool, not an AI coach (for now)

## Status

Built and deployed. See `app-overview.md` for the tech stack actually used
(Vue 3 + Vite, superseding the original "no build step" preference above) and
the README for local testing / deployment instructions.
