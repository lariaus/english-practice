# Tool: Dictionary

## Goal

A quick word-lookup tool, reachable from anywhere in the app - not a screen
you navigate to, but a global popup overlay summonable on top of whatever
you're currently doing. Two ways to use it: look up one specific word
(triggered by a transcript-word click in YT Shadowing, or a flashcard
front/back word click in Flashcards), or search for any word directly (via
the Home screen's "Dictionary" entry, or a global keyboard shortcut).

## Two modes

- **Word mode**: opened for one specific word. Shows that word's info
  directly, no search bar, and no way to switch into search mode from here -
  the two entry paths are strictly separate.
- **Search mode**: opened with no word yet. Shows a search bar at the top,
  always starts blank (even if a previous search is still showing from the
  last time this instance was opened), and looks up on Enter/submit rather
  than live-as-you-type. After a lookup, stays in search mode (the search
  bar stays visible) so you can search again without closing and reopening.

## Triggering it

- **Transcript word click** (YT Shadowing): pauses the video, opens word
  mode for that word - see `yt-shadowing-spec.md`'s "Dictionary lookup"
  section.
- **Flashcard front/back word click** (Flashcards' Learn/Review/Practice
  screens): opens word mode for that word, same `emit('show-word', word)` →
  `App.vue`'s `handleShowWord` path as the transcript click above - see
  `flashcards-spec.md`. Every word is independently clickable (a card like
  "voiture rouge"/"red car" is 4 separate lookups), unlike a transcript
  line's click-to-seek behavior.
- **Home screen "Dictionary" entry**: opens search mode, blank. Unlike the
  other four Home entries (Recorder Loop, Robot Shadowing, YT Shadowing,
  Flashcards), this doesn't navigate to a new screen - see `app-overview.md`.
- **Global keyboard shortcut: Option+Shift+D** - works from any screen, at
  any time. Checked via `event.code === 'KeyD'`, not `event.key` - holding
  Option on Mac makes `key` report a produced special character (`∂` on a US
  layout) rather than `"d"`, while `code` stays stable regardless of
  modifiers held. No-ops if the popup is already open (covers both
  "pressed twice in a row" and "a word's already showing from a transcript
  click") - it never interrupts or steals focus from an in-progress lookup.

## Global ownership

A single instance, mounted once in `App.vue`, sitting alongside whichever
screen is currently active - not owned by any one screen, unlike its
previous life as `WordInfoPopup.vue` living inside `YtShadowingPlayerScreen.vue`
alone. Screens that can trigger it emit an event upward instead of holding a
direct reference to it:

- `YtShadowingPlayerScreen.vue`: `emit('show-word', word)`
- `FlashcardsLearnScreen.vue`/`FlashcardsReviewScreen.vue`/
  `FlashcardsPracticeScreen.vue`: same `emit('show-word', word)`
- `HomeScreen.vue`: `emit('open-dictionary')`

`App.vue` owns the actual `ref` and calls `showWord(word, options)` /
`showSearch()` on it. Its open/closed state is passed back down to any
screen that needs to gate its own behavior while it's open - e.g.
`YtShadowingPlayerScreen.vue` (and the three Flashcards screens above)
receive a `dictionaryOpen` prop, driving their own `:inert` wrapper the
same way, just sourced globally instead of locally now. See
`docs/common-design-philosophy.md`'s "Modal popups" section for the general
"single global owner" pattern this follows - worth reusing for any future
modal that isn't provably confined to one screen.

Opening the dictionary never pauses or otherwise touches whatever's active
on the screen underneath (a playing video, a mid-recording session) - it's a
pure overlay, fully independent of anything else running.

## What the popup shows

Once a word is loaded (either mode, identical either way):

- The word, phonetic transcription(s) labeled by dialect (`US`/`UK`/`AU`, or
  `??` when undetectable), US always listed first when available.
- Up to 2 definitions per part of speech, with examples/synonyms.
- A Wiktionary attribution line (required by the data's CC BY-SA license).
- A play button next to the word, and one per phonetic entry with audio -
  same triangle icon as the video's Play/Pause (`PlayPauseIcon.vue`), but
  these never show the pause variant or toggle anything: a word clip is
  short/one-shot, so there's nothing meaningful to pause. Shift-click or a
  long-press plays that one clip at 0.5x instead of normal speed - no
  "restore afterward" step needed, since each click is an independent
  one-shot play.
- **Record and Shadow buttons** (via the shared `useRecordShadow.js` +
  `RecordShadowButtons.vue`, the same infrastructure YT Shadowing's own
  controls use - identical icons, labels, mic engine, `R`/`S`/`L` and
  `S`/`R`/`L`/`P` codes):
  - *Record*: plain toggle, auto-plays your recording back on stop.
    Shift-click/long-press requests a double record (two full
    record+playback cycles, a beep marking the transition between them) -
    see `yt-shadowing-spec.md`'s "Recording & shadowing" section for the
    full mechanics, shared via `useRecordShadow.js`.
  - *Shadow*: one pass of play-the-word → record → listen-to-yourself. The
    word's own clip is played first, so (unlike YT Shadowing's video-segment
    Shadow, which sizes its replay from your recording afterward) its
    length is already known - that measured length + 0.25s becomes the
    recording window, no guessing needed. Shift-click or a long-press runs
    a second pass immediately after the first, replaying the word again
    rather than reusing the first pass's clip.
- Quick links to open the word in Cambridge Dictionary or WordReference,
  each in a small centered popup window (`externalDictionarySites.js`).

## Closing

X button, clicking the backdrop, or `Escape` - identical in both modes.
Immediately stops and resets any in-progress Record/Shadow session (mic
recording, beeps, playback) - nothing keeps running in the background after
it's gone.

## Data sources

Deliberately **independent of native-server's caption fetching** -
`dictionaryClient.js`
fetches directly from the free, keyless Free Dictionary API
(`api.dictionaryapi.dev`) client-side, with an in-memory cache so repeat
lookups of the same word never re-fetch.

Pronunciation playback (`wordAudioPlayer.js`) tries a real recorded
American-English clip from the dictionary data first, falling back to
Google Translate TTS if none exists for that word - `playWordPronunciationTimed`
is the same fallback chain, awaitable, resolving with the elapsed seconds
once the clip finishes (used by Shadow above).

## Architecture fit

- Component: `DictionaryPopup.vue` (renamed from `WordInfoPopup.vue`),
  mounted once in `App.vue`.
- Triggers: `YtShadowingPlayerScreen.vue` and the three Flashcards session
  screens (`emit('show-word', word)`), `HomeScreen.vue`
  (`emit('open-dictionary')`), and `App.vue`'s own global keydown listener
  for the Option+Shift+D shortcut.
- Engines (plain JS, no Vue, unrenamed - already read fine as
  dictionary/word-related): `dictionaryClient.js` (fetch+cache word info),
  `wordAudioPlayer.js` (pronunciation fallback chain - also exports
  `playTextAloud`/`playTextAloudTimed`, a TTS-only subset with no
  dictionary lookup, used by Flashcards' Play/Shadow buttons for a whole
  phrase rather than a single dictionary word),
  `externalDictionarySites.js` (centered-window openers).
- Shared UI, not owned by this doc: `RecordShadowButtons.vue` +
  `useRecordShadow.js` (Record/Shadow buttons), `PlayPauseIcon.vue`
  (play/pause icon), `useShiftOrLongPress.js` (the shift-click/long-press
  gesture underneath the 0.5x-playback and double-pass behaviors) - see
  `docs/common-design-philosophy.md`.

## Explicitly out of scope / known limitations

- **Dictionary audio coverage is inconsistent** - it's crowdsourced, so not
  every word has a recorded clip, and there's no reliable field indicating
  dialect when a word has no explicitly-US-labeled entry (falls back to
  `??`). See `docs/dictionnary_sources_research.md` for the research behind
  these data-source choices.
- **Search mode has no autocomplete/suggestions** - a typo or misspelling
  just returns "no definition found," same as it always has for any
  unrecognized word. Planned - see "Planned: spelling suggestions" below.
- **No search history** - search mode always starts blank; there's no list
  of recently-searched words to pick back up.

## Planned: spelling suggestions ("did you mean")

Not yet built - notes captured here so the design doesn't need to be
re-derived later. No urgency; revisit whenever.

**What**: when a lookup returns "no definition found," show a "Did you
mean:" list of a few close-spelling alternatives, each clickable to
re-trigger a lookup for that word instead.

**The caveat that shaped this**: two candidate data sources were weighed.

- Datamuse API (`api.datamuse.com/sug`) - free, keyless, does real
  full-word spelling correction (not just prefix autocomplete). Rejected:
  it's a completely unrelated data source from the Free Dictionary API this
  app already uses, so a "close spelling" match from Datamuse isn't
  guaranteed to actually have a real entry in `api.dictionaryapi.dev` -
  clicking a suggestion could lead to another "not found," worse than no
  suggestion at all.
- The Free Dictionary API's own word list - `meetDeveloper/freeDictionaryAPI`
  (the project behind `api.dictionaryapi.dev`) has
  `meta/wordList/english.txt` in its GitHub repo: a plain one-word-per-line
  list, confirmed by direct download to be 233,464 words, 2.5MB raw /
  ~850KB gzipped. Chosen instead of Datamuse specifically because it comes
  from the *same* project, not an unrelated API - much stronger (though not
  ironclad-proven) overlap with what the live API can actually define.
  **Open question, not resolved**: this file isn't explicitly documented as
  "the authoritative list the live API checks against" - it's a reasonable,
  well-grounded inference from the repo structure, not a guarantee. Decide
  at implementation time whether to also validate each suggestion against a
  real `fetchWordInfo()` call before showing it (safer, costs a few extra
  parallel network calls per failed search - the existing cache absorbs the
  cost if the user then clicks one) or trust the list directly (simpler,
  small residual risk of an occasional dead-end suggestion).

**How to build it**:

1. Host a local copy of `english.txt` as a static asset in this repo
   (rather than fetching it live from GitHub at runtime) - avoids adding a
   third external host dependency, and this repo already controls its own
   hosting/compression via GitHub Pages.
2. Fetch it lazily - only the first time a lookup actually fails, not on
   every app load - and cache it in memory afterward (module-level
   variable), same "lazy, fetch-once" pattern already used elsewhere (e.g.
   subtitle fetching in YT Shadowing).
3. On a failed lookup, run a fuzzy/edit-distance match (e.g. Levenshtein
   distance) against the cached word list client-side to find the closest
   few words - fast enough in-browser even at ~233K words (tens of millions
   of simple character comparisons, well under a second on modern JS
   engines); a length/first-letter pre-filter before the full distance
   calculation is a reasonable optimization if it ever feels slow, but
   likely unnecessary.
4. (If keeping the validation safety net) resolve the top candidates
   through `fetchWordInfo()` concurrently, keep only the ones that come
   back with a real entry, cap the shown list at ~5.
5. Display as a "Did you mean:" list in the popup when a lookup fails, each
   entry clickable to re-run the same lookup for that word instead.
