# Mic / audio issues after backgrounding (iOS / WKWebView)

## Status: mitigated, not root-caused

Recoverable now, but recovery is **slow to kick in** - a broken button takes a
few seconds of internal timeout/observation before the engine notices and
resets itself, so the felt experience is "press it, wait a few seconds,
nothing happens, then it works" rather than an instant fix. That's the
tradeoff of the detection approach that ended up being necessary (see
"Why detection had to become behavioral, not state-based" below) - it's not
a bug in the fix, it's inherent to how the fix works, and the main thing
still worth improving if this gets picked up again.

This is not a new class of problem for this app (mic/audio glitches on iOS
have happened before), but it's the first time it's been bad enough to fully
block a session - specifically, mid-`Learn`/`Review`/`Practice` in Flashcards,
requiring an app restart and losing session progress. That severity is why
this got a real investigation instead of a shrug.

## Symptom

After the app sits backgrounded or the screen locks for roughly 30+ seconds
(exact threshold unconfirmed - see "Reproduction" below), returning to the
app and pressing Play, Record, or Shadow on a Flashcards face does
**nothing** - no sound, no error, no visible feedback. Manually navigating
away from the screen and back fixes it (see "Why does back-and-forward fix
it?" below for why that's actually a confusing data point, not a clean one).

## Root causes found (plural - this turned out to be at least three distinct bugs stacked together)

All were confirmed with real on-device evidence, not just theory - see
"Diagnostic approach" below for how.

### 1. iOS mutes the mic track without ending it

`getUserMedia()`'s returned `MediaStream`/`MediaStreamTrack` is normally
checked for "is this stream still usable" via `track.readyState === 'live'`.
On iOS, backgrounding a WKWebView flips Control Center's "Mic Mode" from
"Standard" to "Off" and mutes the mic capture at the OS level - but the
track's `readyState` **stays `'live'`**. Only `track.muted` flips to `true`,
and it does not reliably un-mute itself even once the app is foregrounded
again. A check that only looks at `readyState` (the first fix attempted)
never notices anything wrong.

Confirmed via device diagnostics: a real repro showed
`stream=[live=false,muted=true]` right before recovery kicked in - so in
this case `readyState` had *also* gone false, but `muted` is the property
that's documented (WebKit/Apple Developer Forums) as the one that
specifically reflects this OS-level "Mic Mode: Off" state, and should be
checked regardless of what `readyState` says.

### 2. WebKit's non-standard `AudioContext` `'interrupted'` state

In addition to the standard `'suspended'` state, WebKit has its own
`'interrupted'` state (backgrounding, Siri, phone calls). A `resume()` check
that only handles `'suspended'` never even attempts recovery from
`'interrupted'`. Multiple independent bug reports (WebKit's own GitHub
issues, Apple Developer Forums) also note that `resume()` against
`'interrupted'` is itself unreliable on iOS - it can resolve without the
state actually flipping back to `'running'`.

### 3. `AudioContext.state` and a persistent `<audio>` element can both *lie* - the real headline finding

This is the one that actually explains why the bug survived two rounds of
fixes based on (1) and (2):

- **`AudioContext.state` can report `'running'` while the context is
  genuinely wedged.** Confirmed by a real repro (screenshot-captured device
  diagnostics) where *every single checkpoint* reported healthy - dead
  stream correctly detected and replaced, a fresh `getUserMedia()` came back
  live, the recorder captured real audio (16947 bytes, not silence),
  `decodeAudioData` succeeded (2.00s decoded), and `ctx.state === 'running'`
  at every step - and yet `AudioBufferSourceNode.onended` **never fired**,
  leaving `playBlob()`'s promise pending forever and the UI stuck showing
  "L" (the Shadow button's "listening to your own playback" state)
  indefinitely. There is no property to introspect that would have caught
  this - the context insists it's fine.
- **A persistent `<audio>` element (used for Google TTS playback) can get
  wedged mid-playback and silently resolve `.play()` as if nothing's wrong.**
  If a phrase is interrupted by backgrounding mid-play, iOS can simply never
  deliver the `ended` (or `pause`) event. The element's internal state stays
  frozen: `paused` stays `false` and `currentTime` stays frozen forever, even
  after assigning a brand new `src` on top of it later. Calling `.play()` on
  it then just resolves immediately - the browser thinks it's "already
  playing" - without decoding or rendering a single frame, and **without
  ever rejecting**, so there's no error for a `.catch()` to observe. Confirmed
  by an actual side-by-side diagnostic capture:
  - Broken: `paused=false`, `currentTime` frozen at `1.99 -> 1.99` across a
    400ms window, despite the browser insisting it was "playing."
  - Working: `paused=true` right before the same call.

The practical lesson: **don't trust `.state`/`.paused`/`.readyState` alone
to decide "is this object usable" after a background/foreground cycle on
iOS - verify behavior, not just state.**

## Why detection had to become behavioral, not state-based

Because of finding (3) above, the fix couldn't just be "check the right
property this time" - there wasn't a property left to check. The two
mitigations that actually worked both had to **observe something happening
over a short window of time** instead of reading a flag once:

- **Mic playback (`playBlob()`/`playBeep()`, Web Audio API)**: wrap the wait
  for `onended` in a timeout (`audioBuffer.duration + 2s` for playback,
  `BEEP_DURATION_MS + 1s` for the beep). If `onended` hasn't fired by then,
  give up, resolve anyway (so the UI doesn't hang forever), and set a
  `_audioCtxSuspect` flag that forces the *next* `_ensureAudioContext()`
  call to recreate the context outright rather than trusting `.state` again.
- **TTS playback (`_speakGoogleTts()`, `<audio>` element)**: listen for a
  real `timeupdate` event (only fires on genuine progress) for up to 1.5s -
  but critically, that 1.5s clock only starts once data has actually
  arrived (`readyState` reaching `HAVE_CURRENT_DATA`, via the `loadeddata`
  event), not from the moment `.play()` resolves. No `timeupdate` in that
  post-data window means it's wedged - force a `pause()` + `load()` reset
  and retry once before giving up. Two earlier, narrower versions of this
  check both produced false positives before landing here: comparing
  `currentTime` at a fixed 400ms mark (too short to tell "genuinely wedged"
  apart from "still buffering the network fetch," which routinely takes
  longer than 400ms on its own), then switching to `timeupdate` but still
  starting the clock at `.play()`-resolution time (still wrong on a slow
  connection - the confirmed wedged repro had `readyState=4`, i.e. data
  *already fully loaded*, so gating the check on data having arrived first
  means an arbitrarily slow network never gets misdiagnosed as a bug -
  only "has data, isn't moving" does).

**This is the source of the multi-second detection lag the user flagged as
the main remaining problem.** There's no way to make this instant while
still trusting behavior over state - a faster fix would need either a
shorter timeout (real risk of false positives on slow network/device) or a
fundamentally different signal (see "Ideas for next time" below).

## Why does back-and-forward "fix" it? (an unresolved, slightly confusing data point)

Manually leaving the Practice/Learn/Review screen and returning reliably
fixes all three buttons. That's initially confusing: it destroys and
recreates the **mic** engine (`useRecordShadow.js`'s `onBeforeUnmount` calls
`micEngine.destroy()`, and the new screen mount builds a fresh
`MicRecorderEngine`), which explains why Record/Shadow recover - but the
**TTS engine** behind Play (`wordAudioPlayer.js`'s `getGoogleTts()`) is a
**module-level singleton** that is never destroyed by navigating screens.
The literal same `TTSEngine` object and `<audio>` element persist across the
navigation. If Play was also confirmed broken-then-fixed by this (not
independently re-verified - worth confirming next time), that would mean
object recreation isn't the actual mechanism for the TTS side, and something
about a fresh interaction/navigation itself (page-level "user activation,"
maybe) is what actually un-sticks it - a different mechanism than what fixed
the mic side. Not resolved; flagged here so it isn't re-investigated from
scratch.

## Diagnostic approach that actually worked

Two rounds of blind fixes (readyState-only check, then muted+interrupted
check) both failed to resolve the bug, because of finding (3) above - the
state being checked was itself lying. The turning point was adding
temporary, on-device diagnostics using the app's existing Toast system
(`useToast.js`/`ToastHost.vue`) directly inside `micRecorderEngine.js` and
`ttsEngine.js`, surfacing the actual real-time values (`track.muted`,
`track.readyState`, `audioCtx.state`, `<audio>` element's `readyState`/
`networkState`/`error`/`paused`, recorded byte counts, decode results,
play()-resolved-vs-rejected, progress-after-play) as toasts right at the
moment a button is pressed. This mattered specifically because Safari Web
Inspector (the normal way to get a real console on a WKWebView-embedded app)
is inconvenient mid-repro on a physical device with a background/lock cycle
in the middle of the test. Those diagnostic `showToast()` calls have since
been stripped out of both files (the actual fixes they helped find - dead
stream/wedged-context/wedged-element detection, the timeout safety nets -
are unaffected and still in place) - if this gets picked up again and the
same "state looks fine but isn't" problem resurfaces elsewhere, re-adding
the same kind of temporary on-device toast instrumentation is worth doing
again rather than guessing blind.

## Reproduction

Waiting for iOS's own Auto-Lock timeout is unreliable (any touch resets it,
and the actual duration depends on the Settings > Display & Brightness >
Auto-Lock value, not a fixed 30s). The reliable repro found during this
investigation:

1. Open Flashcards Practice (or Learn/Review), confirm Play/Record/Shadow
   work.
2. Press the iPhone's side button once to lock the screen immediately -
   don't wait for auto-lock.
3. Leave it locked, undisturbed, for 30-60 seconds.
4. Unlock and return to the still-running app (not a relaunch) and retry the
   buttons.

Backgrounding via swipe-to-home-screen (keeping the app alive in the
switcher) was also worth trying as a second repro path if screen-lock alone
doesn't trigger it - the two can produce slightly different iOS suspension
behavior.

## What was fixed (current code state)

- **`src/engine/micRecorderEngine.js`**: single `_ensureInRightState()` gate
  called at the top of every mic/playback entry point (`start()`,
  `recordFor()`, `playBlob()`, `playBeep()`) rather than reacting to
  `visibilitychange` - deliberate, since WKWebView's `visibilitychange`
  firing on background/foreground is itself
  [documented as unreliable](https://github.com/apache/cordova-ios/issues/588)
  in embedded WKWebView contexts (works fine in real mobile Safari). The gate:
  drops a stream whose tracks are dead or muted; resumes (or, if that
  doesn't actually work, recreates) the `AudioContext`, covering both
  `'suspended'` and `'interrupted'`; and now also force-recreates the
  context if a previous playback timed out (see `_audioCtxSuspect` above).
- **`src/engine/ttsEngine.js`**: detects a wedged `<audio>` element via
  `!paused` at the start of a new phrase and force-resets it; detects a
  resolved-but-frozen `.play()` via absence of `timeupdate` events within
  1.5s and retries once before giving up.
- **`src/engine/recorderLoopEngine.js`** / **`src/engine/robotShadowingEngine.js`**:
  same `'interrupted'`-state handling added to their existing
  `_ensureAudioContext()` (lower priority/exposure than Flashcards, since
  both already hold a Wake Lock during an active session, which stops the
  screen auto-locking - though it doesn't stop true app-backgrounding, e.g.
  the user manually switching apps mid-session, so the same class of bug is
  still theoretically reachable there and hasn't been specifically tested).

## Ideas for next time (not yet tried)

- **Shorten detection latency** without reintroducing false positives - the
  main open ask. A timer-drift ("heartbeat") detector
  (`setInterval` comparing expected vs. actual elapsed time) could reveal
  "we were just suspended" faster and more directly than waiting out a
  per-call timeout, since JS timers themselves pause during real
  suspension and the drift on resume is immediately visible - worth
  prototyping as a *faster first signal* that triggers a preemptive reset
  proactively, rather than only reactively timing out on the next button
  press.
- **Native-side lifecycle hooks.** Since `document.visibilitychange` is
  unreliable in WKWebView, consider having the Swift side
  (`english-practice-app/`) observe real
  `applicationDidBecomeActive`/`applicationWillResignActive` notifications
  and forward them into the page via `evaluateJavaScript` - a much more
  precise "we just resumed from background" signal than anything JS-only can
  construct on its own.
- **Native `AVAudioSession` configuration.** Investigate whether configuring
  an audio session category on the native side could prevent WKWebView's mic
  capture from being muted/wedged in the first place, rather than only
  detecting and recovering from it after the fact.
- **Unify Play onto Web Audio API** instead of `HTMLMediaElement`, matching
  the mic engine's approach - `AudioBufferSourceNode`/`AudioContext` gave a
  cleaner (if still imperfect - see finding 3) recovery story than a
  persistent `<audio>` element did; worth considering whether the same
  primitive for both would simplify things, though the no-CORS constraint
  that led to using an `<audio>` element for Google TTS in the first place
  (see `ttsEngine.js`'s own comments) would need to be re-solved.
- **Confirm or rule out** the back-and-forward mystery above - specifically
  whether Play was ever independently confirmed broken-then-fixed by
  navigation alone, given its singleton TTS engine is never recreated by
  that navigation.

## Unrelated bug found via the same investigation (still open, separate issue)

Diagnosing this bug started with a "rename the title and rebuild" cache-busting
test, which surfaced a **separate, unrelated bug**: `native-server` serves
`dist/` with no `Cache-Control`/`Expires` headers at all (confirmed by
reading `native_server_core/lib/server.cpp` and the vendored `httplib.h` -
only `ETag`/`Last-Modified` are set, by cpp-httplib's default static-file
handler, not by any app code). WKWebView's `URLCache` can therefore serve a
stale `index.html` (pointing at an old, no-longer-existing JS bundle)
indefinitely after a rebuild, without even attempting to revalidate over the
network - explaining why an iOS build appeared to not pick up new code while
a macOS build of the same commit did. **Diagnosed but not yet fixed** -
proposed fix is a `set_post_routing_handler` hook in `server.cpp` that adds
`Cache-Control: no-store` specifically for `/` and `/index.html` (leaving the
content-hashed files under `/assets/` cacheable as before, per the standard
SPA caching pattern). Not implemented yet since attention moved to the audio
bug; worth doing independently of anything else in this doc.
