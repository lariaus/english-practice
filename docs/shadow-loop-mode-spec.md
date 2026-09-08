# ShadowLoopMode (hands-free shuffle-and-shadow loop)

## Goal

A generic, hands-free shadowing loop: given a list of texts, repeatedly pick
one at random, run a play → beep → record → beep → play-back cycle (the
same fundamental shape as the existing Flashcards Shadow button and Robot
Shadowing), and keep going until stopped - no per-word button presses
needed once it's running. Wired into two places: Flashcards' Practice
screen (looping over the current set's cards) and YT Shadowing's player
screen (looping over a video transcript's vocabulary) - built generically
enough that both share the same engine and UI, each supplying its own item
list, its own `speak()` callback, and its own entry/exit point.

Per the TODO item this replaces (`TODO.md`'s "hand-free mode... based on
difficulty/results"): the difficulty/results-aware selection is explicitly
out of scope here - selection is fully random, with no notion of which
words you find harder.

## How it works, end to end

### Entering and leaving loop mode

Each host screen owns its own entry point and its own "how do I get back"
navigation - ShadowLoopMode itself has no opinion on either:

- **Flashcards' Practice screen**: a `Loop` button sits in the header next
  to the `Practice` title (in place of the spacer that normally balances
  the Back button's width). Clicking it switches the screen's main content
  away from the normal flip-card/grade-buttons view into loop mode. The
  same header button relabels to `Exit` while in loop mode; clicking it
  returns to the normal card view. Entering/exiting never touches or resets
  the normal practice flow's own state (its shuffled card order, current
  index, or flip/reveal state) - the two are entirely independent.
- **YT Shadowing's player screen**: a `Loop (words)` button sits in the
  advanced/hands-free controls row (behind the same expand toggle as the
  existing HF/HF² buttons). Clicking it pauses the video (if playing) and
  replaces the whole player view (video, controls, transcript panel) with
  loop mode. A `← Back to YT Shadowing` button at the top of that view
  returns to the normal player - the video stays paused where you left it,
  nothing auto-resumes.

In both cases, loop mode fully replaces the host's own main content (rather
than appearing as an overlay alongside it) - the same way Robot Shadowing is
its own dedicated screen rather than something layered on top of another
one.

### The options screen

Loop mode always shows three controls, styled identically to Robot
Shadowing's own options (same shared global CSS classes, no new styling
needed):

```text
Repeat        [1] [2] [3]
Repeat model: Off/On
[ Start ]
```

- **Repeat** (1/2/3): how many times the *same* item is shadowed
  back-to-back before a new random one is picked. This is a repeat count
  per item, not a "how many laps through the whole list" setting.
- **Repeat model** (Off/On): when On, the item's own audio plays a second
  time immediately after recording, right before you hear your recording
  played back - a refresher listen of the correct pronunciation right
  before comparing it to your own attempt. Off skips straight from
  recording to playback.
- **Start**: begins the loop. While a run is active, all three controls
  stay visible but disabled (so you can see what's configured without being
  able to change it mid-run), and the Start button turns into a red **Stop**
  button in the same spot. Stop halts the current run and returns you to
  this same options screen, still in loop mode - it does not exit loop mode
  itself; only the host's own Exit/Back navigation does that.

### What happens once you hit Start

The loop repeatedly does the following, forever, until you hit Stop:

1. **Pick an item.** Fully random, from every item in the current list,
   excluding only the immediately-previous pick (so the same item can't
   repeat twice in a row, but there's no guarantee every item gets shown
   before any repeats - one could come up several times in a row before
   others are ever picked). This mirrors Robot Shadowing's own selection
   algorithm exactly.
2. **Show it.** How it's displayed is entirely the host's choice (see
   Architecture below) - Flashcards shows both the front and back of the
   picked card (like looking at a normal, already-flipped flashcard); YT
   Shadowing shows just the bare word. Either way, what's shown is
   independently clickable, opening the global Dictionary popup, exactly
   like every other clickable word in this app. A phase-status label above
   it shows what's currently happening (`Playing…` / `Listening…` /
   `Playing back…`), colored the same way Robot Shadowing's own phase label
   is.
3. **Play it.** How an item's audio actually gets played is also the host's
   choice, via its own `speak()` callback - Flashcards strips annotations
   from the card's front and prefers real dictionary audio for a single
   word (TTS for a phrase); YT Shadowing's transcript words are already
   confirmed to have real US dictionary audio (see below), so it always
   plays that directly.
4. **Beep, then record.** The recording window is sized to however long the
   audio in step 3 actually took, plus 0.75 seconds - so there's always a
   little extra room to finish speaking without feeling cut off, without
   having to guess a fixed duration up front.
5. **If Repeat model is On:** beep, then play the item's audio again (the
   "refresher" - a chance to hear the correct pronunciation immediately
   before hearing your own attempt).
6. **Play back your recording.** If Repeat model was Off, this is preceded
   by a beep (marking the transition out of the silent recording window).
   If Repeat model was On, there is **deliberately no beep** here - going
   from "listening to the model's audio" straight into "listening to your
   own recording" is two passive listens back to back, with nothing for you
   to *do* in between, so there's nothing for a beep to usefully cue. Every
   other transition in this sequence keeps its beep.
7. **Repeat or move on.** If the configured Repeat count hasn't been
   reached yet for this item, go back to step 3 with the same item.
   Otherwise, go back to step 1 and pick a new one.

## Architecture

### `src/composables/useShadowLoop.js` - the generic engine

A Vue composable that owns the state machine above, generalized over an
opaque list of caller-supplied items instead of a baked-in phrase set - the
piece that makes it reusable across screens.

```js
const shadowLoop = useShadowLoop({ speak: async (item) => { /* ... */ } })
// shadowLoop.isActive, shadowLoop.currentItem, shadowLoop.loopPhase
// ('idle' | 'speaking' | 'recording' | 'replaying-model' | 'playing-back')
// shadowLoop.start(items, { repeatCount, repeatModel }), shadowLoop.stop()
```

`speak(item)` is entirely the caller's responsibility - it decides *how* to
play an item's audio and resolves with how many seconds that took. The
engine itself has no opinion on audio sourcing, only on sequencing.

Deliberately built directly on `MicRecorderEngine`'s existing `playBeep()`/
`recordFor()`/`playBlob()`/`destroy()` primitives - the same low-level mic
plumbing `useFlashcardFaceAudio.js`'s own single-card Shadow button already
uses - rather than reimplementing MediaRecorder/beep handling from scratch
the way `robotShadowingEngine.js` does independently, and rather than
pulling in `useRecordShadow.js`'s higher-level Record-button-specific
plumbing (gesture detection, double-record, R/S/L labels), which has no
equivalent need here. Each host gets its own fresh `MicRecorderEngine`
instance, never shared with anything else on that screen.

Stopping calls `micEngine.destroy()` (not the narrower `micEngine.stop()`),
so a Stop press cuts off whatever is actively playing right now - a beep,
the word/phrase audio, or a recording/playback - immediately, rather than
only being able to interrupt an in-progress recording. A later Start
transparently re-acquires the mic stream/AudioContext, the same as any
other post-`destroy()` use of `MicRecorderEngine` elsewhere in the app.

### `src/components/ShadowLoopPanel.vue` - the shared UI

Owns the options screen and the running view (phase label, Repeat/Repeat
model/Start-Stop, and the current item's container) - everything from "the
host has decided to enter loop mode" onward. Takes two props:

```vue
<ShadowLoopPanel :items="items" :speak="speakCallback">
  <template #current-item="{ item }">
    <!-- host-specific rendering of the current item -->
  </template>
</ShadowLoopPanel>
```

- `items`: the array to loop over (opaque to this component).
- `speak`: the per-item audio callback, passed straight through to
  `useShadowLoop`.
- `#current-item` scoped slot: how to actually display the current item -
  deliberately left to the host, since a flashcard's front+back is a very
  different shape from a bare transcript word.

The host owns its own entry-point control (a header Loop/Exit toggle, a
Loop-in-advanced-options button + a Back button) and just conditionally
mounts this component via `v-if`. No explicit "exit" event is needed -
unmounting the component tears down its internal `useShadowLoop` instance
automatically (its own `onBeforeUnmount`), which stops and cleans up
whatever was running.

### Flashcards' integration (`FlashcardsPracticeScreen.vue`)

The item list is rebuilt fresh each time loop mode is entered, from the
same `cards` array the normal practice flow already holds:

```js
const loopItems = computed(() =>
  cards.value.map((card) => ({
    front: card.front,
    back: card.back,
    speakableText: stripAnnotations(card.front),
  })),
)
```

The `speak` callback mirrors `useFlashcardFaceAudio.js`'s own dispatch rule
exactly (that function's `isSingleWord` helper is exported specifically so
this doesn't have to duplicate it) - only the **front** ever drives audio;
the back is shown for reference only, never spoken or recorded against:

```js
async function speakLoopItem(item) {
  return isSingleWord(item.speakableText)
    ? await playWordPronunciationTimed(item.speakableText)
    : await playTextAloudTimed(item.speakableText)
}
```

The `#current-item` slot renders both the front and back via the existing
`<FlashcardText>` component (unchanged, reused as-is), which already
handles annotation display and per-word clickability, emitting `word-click`
the same way the normal card view does - flowing into the same
`handleWordClick` → `emit('show-word', word)` path already wired to
`App.vue`'s global Dictionary popup for this screen.

### YT Shadowing's integration (`YtShadowingPlayerScreen.vue`)

Clicking `Loop (words)`:

1. Pauses the video if it's playing.
2. Ensures the transcript is fetched (`ensureSubtitlesFetched()` - reused
   as-is; a no-op if the CC button or Auto Shadow already fetched it this
   session).
3. Extracts every unique clickable word across every transcript cue, using
   the exact same tokenization (`splitIntoWords`/`isClickableWord`/
   `cleanWord`, lowercased) already used for the transcript's own
   clickable-word rendering - no new tokenization rules.
4. Sends that word list to a new native-server endpoint,
   `POST /dictionary/us-audio-words`, which filters it down to just the
   words that already have a cached, real US pronunciation recording (see
   below) - `fetchUsAudioWords()` in `nativeServerClient.js`.
5. If nothing matched, shows a plain "no words with cached US audio yet"
   message instead of the loop panel. Otherwise, shows `ShadowLoopPanel`
   with one item per matched word (`{ word }`), whose `speak` callback is
   simply `playWordPronunciationTimed(item.word)` - no TTS fallback or
   single-word/phrase branching needed, since every item here is already a
   confirmed single word with real audio.
6. The `#current-item` slot renders the bare word as a clickable span
   (reusing the same global `.clickable-word` styling `FlashcardText.vue`
   uses), wired to the screen's own existing `handleWordClick`.

#### Why the bulk filter is cache-only, and why that's a new server endpoint

A transcript can easily have 50-200+ unique words. Checking each one's real
US audio via the full dictionary lookup can mean several live network calls
per word for anything not already cached (Wiktionary's tocdata/wikitext/
imageinfo/audio calls, plus FreeDictionaryAPI) - and Wiktionary already
rate-limits (HTTP 429) under bursty traffic (see `docs/dictionary-spec.md`).
Doing that live, in bulk, for every video would be slow and risks getting
rate-limited. So this filter is **deliberately cache-only**: it only
reports words that are *already* cached with real audio, and never makes a
live network call for anything, ever. Coverage grows naturally as you use
the dictionary/crawler more over time (e.g. crawling a big word list ahead
of time, or just having looked words up before) - a video you've never
explored before may return few or no words, which is expected, not a bug.

Since checking cache-only is a pure disk read (fast, no rate-limit
exposure), a single blocking HTTP request is enough - no background job or
progress polling needed here, unlike Data Pack Sync's own sync mechanism.

New pieces on the native-server side - deliberately built as a genuine
cache-only *mode* threaded through the existing merge machinery, not a
separate hand-rolled path that would duplicate `fetch()`'s own cache-reading
and merge logic:

- `DictionaryEntry::fetchFromCache(word, serverDataDir, languageCode)` -
  the actual thing the bulk endpoint calls. Checks the fused
  `dictionaries/entries/` cache first (the exact same check `fetch()`
  itself does before ever going live); if that misses, reassembles from
  each source's own per-source cache instead of giving up - this matters
  because `dictionary-crawler` deliberately only ever populates the
  per-source caches, never the fused one, so a freshly-crawled word would
  otherwise never match here. Goes through the same
  `mergeDictionaryEntries()` (the phonetics-richness-aware merge) as a live
  `fetch()` would, and persists the reassembled result back to the fused
  cache too, so a later call - including `fetch()` itself - gets a direct
  hit. Never touches the network under any circumstance.
- `WiktionaryAPIEntry::fetchFromCache()` / `FreeDictionaryAPIEntry::fetchFromCache()`
  - the per-source counterparts `fetchFromCache()` above calls (via a new
    `detail::fetchCachedEntryOf<SourceClass>()` template, the cache-only
    sibling of the existing `fetchEntryOf<SourceClass>()` used by `fetch()`
    itself). Each reads that source's own cache file, if any, using the
    exact same JSON parsing its own `fetch()` uses - never falls through to
    a live request under any circumstance, unlike `fetch()`'s normal
    cache-then-live-fallback behavior.
- `POST /dictionary/us-audio-words?lang=<languageCode, default "en">`
  (`native_server_core/lib/dictionary_route.cpp`) - JSON body
  `{"words": [...]}`, response `{"words": [...]}` (the filtered subset,
  each word normalized the same way `DictionaryEntry::fetch()` itself would
  - trim + lowercase + the same small proper-noun exceptions). For each
  word, calls `fetchFromCache()` and checks `.hasEnUsAudio()` on the result.
  `400` for a malformed body or a missing `words` array.

## File map

- `src/composables/useShadowLoop.js` - the generic engine.
- `src/components/ShadowLoopPanel.vue` - the shared options/running UI,
  embedded by both host screens.
- `src/screens/FlashcardsPracticeScreen.vue` - header `Loop`/`Exit` button,
  item-list construction, `speak` callback, `#current-item` slot content.
- `src/screens/YtShadowingPlayerScreen.vue` - `Loop (words)` button in the
  advanced controls row, transcript-vocabulary extraction, the bulk-filter
  request, `speak` callback, `#current-item` slot content, the
  `← Back to YT Shadowing` exit button.
- `src/engine/nativeServerClient.js` - `fetchUsAudioWords()`.
- `native-server/dictionary_utils/include/dictionary_utils/dictionary_entry.h`,
  `native-server/dictionary_utils/lib/dictionary_entry.cpp` -
  `DictionaryEntry::fetchFromCache()`.
- `native-server/dictionary_utils/include/dictionary_utils/wiktionary_api_entry.h`,
  `native-server/dictionary_utils/lib/wiktionary_api_entry.cpp`,
  `native-server/dictionary_utils/include/dictionary_utils/free_dictionary_api_entry.h`,
  `native-server/dictionary_utils/lib/free_dictionary_api_entry.cpp` - each
  source's own `fetchFromCache()`.
- `native-server/dictionary_utils/lib/dictionary_entry_detail.h` -
  `detail::fetchCachedEntryOf<SourceClass>()`.
- `native-server/native_server_core/lib/dictionary_route.cpp`/`.h` -
  `POST /dictionary/us-audio-words`.
- `native-server/tests/integration/test_dictionary_route.cpp` - offline
  tests for the new route (a pre-seeded fake cache file, no real network).
- `src/composables/useFlashcardFaceAudio.js` - `isSingleWord` exported for
  reuse (no behavior change to the file's own Shadow button).
- Reused entirely as-is: `src/components/FlashcardText.vue`,
  `src/engine/flashcardAnnotationSegmenter.js` (`stripAnnotations`),
  `src/engine/wordTokenizer.js`, `src/engine/micRecorderEngine.js`,
  `src/engine/wordAudioPlayer.js`
  (`playWordPronunciationTimed`/`playTextAloudTimed`), `App.vue`'s existing
  `@show-word` → global `DictionaryPopup` wiring.
