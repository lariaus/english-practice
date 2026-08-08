# Flashcards

A new spaced-repetition flashcards tool, alongside the app's other tools
(YT Shadowing, etc.), backed by the same Cloudflare Worker/KV sync
infrastructure used elsewhere in the app (see `docs/cross-device-sync.md`).
Real spaced repetition from day one, using Anki's actual legacy
scheduling algorithm for reviews - see `docs/anki-algorithm.md` for the
exact constants and formulas that document drives. This app deliberately
does **not** implement Anki's real learning-steps mechanic; Learning uses
a simpler custom mechanic described below.

## Data model

```text
FlashcardEntry {
    uid: int              // sequential per set, server-assigned, never reused (see next_uid)
    front: str             // question/prompt side
    back: str              // answer side

    // Scheduling fields - see docs/anki-algorithm.md for the exact
    // constants these are driven by.
    state: enum { NEW, LEARNING, REVIEW, RELEARNING }
    ease_factor: float     // starts at 2.5 (250%), floor 1.3 (130%), no ceiling - meaningful once state != NEW
    interval_days: int     // current interval, rounded to nearest whole day, no maximum cap - meaningful once state != NEW
    due: date              // calendar date the card is next due (due when today >= due) - a date, not a timestamp
    step_index: int        // position within a learning/relearning step list - kept for future-proofing, unused in v1
    lapses: int            // cumulative Again-while-in-Review count, never reset - drives leech detection
}

FlashcardsSet {
    name: str
    cards: list[FlashcardEntry]
    next_uid: int          // monotonic counter for assigning new uids, never reused even after deletion
}
```

`state` is the single source of truth for "is this card learned"
(`learned` == `state != NEW`) - there's no separate `learned` bool.
Although the enum has four values, only `NEW` and `REVIEW` are ever
actually persisted in v1 (see "Learning" and "Review" below for why
`LEARNING`/`RELEARNING`/`step_index` never get written to storage). The
fuller model is kept anyway as future-proofing, in case a later version
wants to persist mid-session progress across app restarts.

Neither struct carries any other metadata (no `created_at`, no
description, etc.) - sets can't be renamed, and the set-picker is ordered
by creation, which the index key (below) already captures for free.

Every word within a card's front/back is always independently clickable to
open the Dictionary popup for just that word (e.g. "voiture rouge" is 4
separate lookups, not one lookup for the whole text) - there used to be a
per-card `is_word` toggle gating this, but it was removed as pointless
friction: unconditionally clickable is just strictly more useful.

## Storage

One Cloudflare KV key per set, keyed by set name - not one blob for the
whole `db` - since KV cross-device sync limits (25MB max per key,
1 write/second per key, see `docs/cross-device-sync.md`) would otherwise
make unrelated sets compete for the same key's write-rate limit, and
reading one set would mean pulling every set's data. 25MB is far more
than any one set needs.

A separate, small index key holds just the ordered list of set names
(append-only - a new name is pushed to the end on `create_set`, nothing
else needs to touch it). `list_sets()` reads this directly rather than
using KV's own `list()` call, since that's paginated and
eventually-consistent. The index deliberately carries *only* names, not
per-set summary data (card counts, due counts) - see "API" below for why.

A live sync server is required for every *mutating* action (creating a
set, adding/editing/deleting a card, Learn, Review, Import CSV, Export
CSV, Reset set) and for Learn/Review's own reads - none of those have an
offline fallback. Viewing a set's content does, though - see "Offline
read-only cache" below.

## Offline read-only cache

The DB is always tried first for every read - the cache below is a
fallback, consulted only on a genuine connectivity failure (no sync
server configured, or the request itself couldn't reach it), never on a
real error from a reachable server (e.g. a 404 for a genuinely-deleted
set still surfaces as an error, exactly as if there were no cache at all).

Backed by `StorageMap` (`src/engine/flashcardsOfflineCache.js`, map
`'flashcards'` - see `docs/local-storage.md`), and deliberately minimal:
only `{uid, front, back}` per card is ever cached - no scheduling fields
(`state`/`ease_factor`/`interval_days`/`due`/`step_index`/`lapses`), and
no derived counts (learned/due) either, since none of that matters for a
read-only view. This also means `mark_cards_learned`/
`send_review_results` never touch the cache at all - they only ever
change scheduling fields.

Every cache write is a **full replace** of whatever unit changed, never a
merge: a whole set's card list is fully overwritten on every successful
`get_set`/`create_set`/`import_csv`, and a single card's `add_card`/
`edit_card`/`delete_card` directly applies that one exact change to the
cached list. A merge could leave stale cards lingering that were
legitimately deleted server-side (e.g. from another device); a full
replace can't have that problem.

What actually works with no connection: viewing the set list (SetPicker),
viewing a set's card content read-only (the Edit screen's grid, with
Add/Edit/Delete/Import/Export/Reset all disabled), and Practice's browsing
(its grading is already inert - no server write happens from grading
regardless of connectivity, and its own inline Edit/Delete are disabled
same as the Edit screen's). The Edit screen's header badge falls back to a
plain `<count> cards` instead of `<learned>/<total> learned` while
offline, since learned/due status isn't cached; Learn/Review swap their
`(count)` label for `Learn Flashcards`/`Review Flashcards` plus a
"no connection" icon instead of showing a stale or zeroed count.

## Dates, not timestamps

Every API that needs "what day is it" takes a plain calendar date string
(`today: date`, e.g. `"2026-01-28"`), supplied by the client - never a
unix timestamp, and never derived by the server from its own clock.

This is deliberate, for two independent reasons:

- **The server can't know your local day - only you can.** "What day is
  it for you right now" is inherently client-side information, especially
  while traveling across timezones (crossing timezones can make your next
  local day start anywhere from a few hours to over a day after your last
  session - there's no minimum real-time gap between two different
  calendar days for you). Since this is a single-user app, trusting the
  client here is low-stakes: a wrong device clock only ever costs you a
  messed-up personal schedule, never a security problem, and real Anki
  itself works the same way (fully local-first, no server-clock concept
  at all).
- **A plain date avoids needing any timezone/rollover-hour logic at all.**
  If the server instead received an epoch timestamp and had to derive
  "which calendar day" from it, it would need a rollover-hour convention
  (real Anki uses a local 4am rollover, not midnight) - entirely
  avoidable by just having the client state its current date directly, no
  derivation needed anywhere.

`FlashcardEntry.due` is a calendar date for the same reason - a card is
due exactly when `today >= due`.

## API

```text
list_sets() -> list[str]
```

Just names, in creation order (the index key's own order) - no summary
data. `total_cards`/`learned_count`/`to_review_count` aren't returned here
because `to_review_count` depends on today's date, not just on what was
last written - caching it in the index would make it silently go stale
the moment a day boundary passes, and keeping it fresh would mean every
single set's content-mutating call also has to write the *shared* index
key, reintroducing the very write-contention problem the per-set-KV split
was meant to avoid. Instead:

```text
get_set(name: str) -> FlashcardsSet
create_set(name: str) -> FlashcardsSet
delete_set(name: str)
```

Tapping a set name in the picker calls `get_set` and computes a
`SetSummary { total_cards, learned_count, to_review_count }` client-side
on the spot (every `FlashcardEntry` already carries `due`, so
`to_review_count` is just counting cards where `due <= today`) - always
fresh, never cached anywhere. `create_set` errors if `name` already
exists; sets can't be renamed (no API for it), so this is the only place a
naming conflict can occur.

```text
add_card(set_name: str, front: str, back: str) -> int
edit_card(set_name: str, entry_uid: int, front: str, back: str)
delete_card(set_name: str, entry_uid: int)
```

Content fields only - never scheduling fields, and never a raw `uid` on
create. The server assigns `uid` on `add_card` (using and incrementing
`next_uid`) and returns it directly, and always owns every scheduling
field itself (defaulted on create, left untouched on edit).

```text
reset_set(set_name: str) -> FlashcardsSet
```

Puts every card's scheduling fields back to their `add_card` defaults
(`state: NEW, ease_factor: 0, interval_days: 0, due: null, step_index: 0,
lapses: 0`), leaving `front`/`back`/`uid` (and the set's own `next_uid`)
untouched - "start this set's learning over" without losing the cards
themselves. Exposed as a "Reset set" button at the bottom of Edit mode,
behind the same confirmation dialog used for deleting a set.

```text
LearnedCard {
    uid: int,
    grade: enum { GOOD, EASY },  // which path graduated it - AGAIN/HARD never graduate a card
}

mark_cards_learned(set_name: str, today: date, learned: list[LearnedCard])
```

Called once a Learning session's batch is done (see "Learning" below) -
only fully-learned cards are ever included. `grade` is `GOOD` for a normal
two-Goods-in-a-row graduation, `EASY` for an Easy-triggered instant
graduation; the server uses it to pick the graduating interval (1 day for
`GOOD`, 4 days for `EASY`, per `docs/anki-algorithm.md`), setting
`due = today + interval_days` and `ease_factor = 2.5` either way. This
call is naturally idempotent - resubmitting the same batch (e.g. after a
dropped network response) produces the identical result, since the
outcome is a pure function of `(today, grade)`.

```text
get_to_review_cards(set_name: str, today: date) -> list[int]
```

Just the uids of cards due today or earlier (`due <= today`), fetched in
batches the same way Learning is (see "Review" below).

```text
ReviewResult {
    card_uid: int,
    grade: enum { HARD, GOOD, EASY },  // never AGAIN - see "Review" below
    lapsed: bool,                       // failed at least once this session before passing
}

send_review_results(set_name: str, today: date, results: list[ReviewResult])
```

Sent once a review batch is fully settled (see "Review" below). Per
result: if `lapsed`, apply the ease penalty/lapse increment once, then the
post-lapse interval reset; otherwise apply normal ease/interval math for
`grade`. Either way, the new `due = today + new_interval`, anchored to the
*actual* review date, never to the card's old `due` - so a late review
still gets the full computed gap starting from when you actually did it,
and overdue cards don't drift or compound oddly.

### Error handling

Every API errors on an unknown `set_name`, and on an unknown
`entry_uid`/`card_uid` within a valid set (`get_set`, `add_card`,
`edit_card`, `delete_card`, `mark_cards_learned`, `get_to_review_cards`) -
no silent no-ops for a stale/nonexistent reference, consistent with
`create_set`'s name-collision behavior.

**One deliberate exception**: `send_review_results` silently skips (never
errors on) any `ReviewResult` whose `card_uid` isn't currently due -
whether that's a genuinely unknown/deleted uid, or a perfectly valid card
that's simply already been processed. This is what makes a retried
submission safe: if a request actually succeeds server-side but its
response is lost (dropped connection, timeout) and the client resends the
same batch, the first application already pushed `due` into the future,
so the retry finds nothing left to apply and silently no-ops instead of
double-applying the ease/interval math.

### Testing

All tests run against **Miniflare** (Cloudflare's local Workers/KV
emulator) - never the real deployed Worker. Miniflare doesn't enforce the
production 1-write/second-per-key limit and isn't eventually consistent,
so tests can freely chain rapid writes to the same set with no rate-limit
or flakiness risk, and since it's fully local, it never touches
Cloudflare's real infrastructure or daily free-tier quotas no matter how
often the suite runs. Each test still creates its own randomly-named set
and tears it down after, keeping tests independent of each other.

## Learning (New → learned)

A **from-scratch mechanic, not Anki's real timed learning-steps model** -
no ease factor, interval, or step timers involved at all.

Cards are learned in batches of a fixed constant, 15 (changeable in code,
not a user-facing setting) - fewer if a set has less than 15 unlearned
cards left. The client picks 15 random `NEW` cards from a `get_set`
fetch (order doesn't matter). For each card: flip to see the back, then
grade with the same four buttons as Review (AGAIN/HARD/GOOD/EASY), driving
a per-card **"good streak" counter that's purely client-side and never
synced**:

| Button | Streak | Outcome |
|---|---|---|
| Again | → 0 | reinserted 5 cards later in the pool |
| Hard | unchanged | back of the pool |
| Good | +1 | learned if streak now reaches 2, else back of the pool |
| Easy | - | learned immediately, regardless of streak |

Cards keep cycling through the pool until every card in the batch is
learned, or the user exits early. Nothing about a card's in-progress state
(streak count, or a card never yet attempted) is ever reported to the
server - only fully-learned cards, via `mark_cards_learned` (see "API"
above), whenever the session ends (full batch done, or an early exit with
only the learned ones sent).

The dictionary popup is available here too, same as everywhere else in the
app.

## Review (spaced repetition)

Follows Anki's real **legacy, SM-2-based** scheduler (not FSRS, Anki's
current default since 23.10) - see `docs/anki-algorithm.md` for its exact
constants/formulas. The **one
deliberate deviation** from real Anki: learning/relearning step timers are
forced to exactly 0 - no real-time waiting anywhere. Everything else (ease
deltas, interval multipliers, the ease floor, lapse handling) is faithful.

A review session fetches due uids via `get_to_review_cards`, in batches of
15 (same constant/pattern as Learning), one review per set (no combined
cross-set session). Skipped days simply pile overdue cards up together,
same as real Anki.

Grading uses AGAIN/HARD/GOOD/EASY. Same reinsertion scheme as Learning
(5 cards later in the queue), minus the streak - Review is simple
pass/fail, not 2-in-a-row. A card's outcome depends on whether it's still
on its first attempt this session, or already recovering from a lapse:

| Button | First attempt this session | Recovering from a lapse |
|---|---|---|
| Again | lapses - reinserted 5 cards later | reinserted 5 cards later (still recovering) |
| Hard | completes normally, ease/interval math for Hard | reinserted 5 cards later (behaves like Again - see `docs/anki-algorithm.md`) |
| Good | completes normally, ease/interval math for Good | completes with a **penalty**, not Good's normal math: ease −20%, `lapses` +1, interval reset to a flat 1 day, `lapsed: true` |
| Easy | completes normally, ease/interval math for Easy | completes with the same penalty as Good above (the passing grade doesn't change the penalty), `lapsed: true` |

**Getting Again then Good is not the same outcome as getting Good
directly** - the first attempt's Again is what triggers the penalty
above, permanently, regardless of what grade eventually completes the
card. A card that lapsed once this session always ends up worse off
(lower ease, reset interval) than if it had passed on the first try.

So a card only ever ends up back in the queue after an Again (first
attempt) or an Again/Hard while recovering - it keeps coming back around
in the *same* sitting, no waiting, however many attempts that takes, until
a Good or Easy finally completes it. Only **Good** or **Easy** can
actually graduate a card back out of Relearning into Review.

A card is only included in what's sent once it's actually been passed; if
the session ends early, still-failing cards simply aren't sent - that
in-progress lapse is silently and permanently lost (no ease penalty, no
lapse increment, no interval change - the card just stays exactly as it
was, still due next time). This is an accepted, low-complexity tradeoff
for v1.

What's sent per completed card is a **compact result, not raw
attempt-by-attempt history** - because the algorithm only applies the ease
penalty and `lapses` increment once per lapse episode (on the first
Again), not again on each subsequent Again before finally passing. So the
server only needs to know (a) whether at least one failure happened before
the pass, and (b) the final passing grade - see `ReviewResult` in "API"
above. A card that lapsed this session can only reach the server with
`grade: GOOD` or `grade: EASY` - never `HARD`, since Hard can't complete a
recovery.

Server-side, a lapsed result applies the ease −20%/`lapses`+1 penalty
once, then the post-lapse interval reset (`old_interval × 0`, floored at a
1-day minimum - the same fixed reset regardless of the passing grade, no
extra easy bonus documented for this case). A non-lapsed result applies
normal ease/interval math for its grade. Either way `due` is computed as
described in "API" above (anchored to today, rounded to the nearest whole
day, uncapped).

### Leeches

`lapses` is tracked per card (needed for scheduling regardless of
anything else) and a leech is just a computed `lapses >= 8` check - no
separate stored field, no manual-clear capability. This is purely
internal bookkeeping: **nothing about leech status is ever surfaced in
the UI** in v1 (no badge, no different treatment, no auto-suspend) -
matching Anki's actual default (tag-only, despite its manual prose
suggesting auto-suspend is the default - see `docs/anki-algorithm.md`).

### Fuzz

Real Anki adds small randomization to computed intervals to avoid
review-load clumping. Fully documented in `docs/anki-algorithm.md` for
future reference (exact ranges, distribution, seeding), but **not
implemented in v1** - all intervals are the plain deterministic numbers,
un-fuzzed. Revisit only if review-load clumping actually becomes
noticeable in practice.

### Why `LEARNING`/`RELEARNING`/`step_index` never get persisted

Tracing through the design above: a card is `NEW` until
`mark_cards_learned` reports it, at which point it goes straight to
`REVIEW` - `LEARNING` is never something the server stores, since the
whole "good streak" mechanic is client-side and ephemeral. Likewise, a
lapsed-then-recovered card fully resolves *within* the same Review
session before anything is sent (the compact `{grade, lapsed}` result),
and an unresolved lapse at session-end is deliberately discarded rather
than recorded - so `RELEARNING` never gets written either. In practice,
the server only ever sees `NEW` or `REVIEW`; the fuller `state` model
exists only as future-proofing.

### No conflict resolution

Multi-device conflicts (learning/reviewing the same set on two devices
before either syncs) are treated as practically impossible for a
single-user app, and not worth the complexity - the server just applies
whatever it receives, with no state comparison beforehand. The one
exception that incidentally also guards against this is
`send_review_results`'s "skip if not currently due" rule (see "API"
above), which happens to make the rare concurrent-write case harmless too,
without any dedicated conflict-detection mechanism.

## Practice (browse mode)

Browses *all* cards in a set (new and learned alike), one at a time in
random order, reshuffling once the pool is exhausted. Same full-screen
flashcard and flip animation as Learn/Review; flipping reveals front and
back together (divided by a rule), not just the back alone. Edit/Delete
buttons sit as an overlay in the card's top-right corner, opening an
inline edit form in place of the card. AGAIN/HARD/GOOD/EASY buttons are
present and visible from the start (not gated behind flipping) but
currently **inert** - pressing any one just advances to the next random
card without affecting scheduling in any way. TODO for a future version.
No progress dots here - there's no fixed batch to track progress through.

## Edit mode

Where cards get added, edited, and deleted (not a separate "add" mode).
Editing sends `edit_card` only on an **explicit Save action per card**,
never continuously while typing - this naturally keeps writes to a set's
KV key well-spaced (a human tapping through cards one at a time is far
slower than the 1-write/second limit), so no batching/debouncing is
needed for v1.

Adding a card checks the new front text (case-insensitively) against every
existing card's front **in the same set** - a match doesn't block the add,
it just confirms first ("Another flashcard already has the front "X". Are
you sure you want to add a duplicate?", via the same `ConfirmDialog` used
for deleting/resetting a set). Client-side only, checked against whatever
`cards` the Edit screen already has loaded - not a server-side constraint,
and not checked across other sets (the same front in two different sets is
normal, e.g. "hello" in both a Greetings set and a Common Words set).
Editing a card's front to collide with another isn't checked - only the
add flow is, for now.

Also where CSV import/export live - two buttons, "Import CSV" (opens a
file picker) and "Export CSV" (downloads a file).

### CSV format

```text
Front,Back
Bleu,Blue
Rouge,Red
```

Just two columns, matched by header name (case-insensitive, any order or
position) - extra columns (e.g. a trailing empty one some spreadsheet
exports add) are ignored. **Export drops every scheduling field entirely**
(only `front`/`back` ever appear in the CSV).

### Why most of this logic lives server-side, not in the browser

Both directions are deliberately thin on the client and do the real work
in the Worker, via two new endpoints sharing one path -
`GET /flashcards/sets/:name/csv` (export) and
`POST /flashcards/sets/:name/csv` (import) - **using raw CSV text as the
request/response body, not JSON**. This is different from every other
endpoint in this spec, and deliberate:

- **Testability**: since the CSV parsing/serialization logic lives in the
  Worker (`cloudflare-worker/src/csvFlashcards.js`), it gets covered by
  the same Miniflare test suite as everything else, rather than being
  untested browser-only code (this app has no frontend test runner).
- **Import must be one batched request, not N calls to `add_card`** - for
  the same reason `mark_cards_learned`/`send_review_results` are already
  single batched calls: each `add_card` does its own read-set/mutate/
  write-set cycle against the same KV key, so looping it once per CSV row
  would both risk the 1-write/second limit and risk two overlapping
  read-modify-write cycles racing each other's `next_uid` bump. The import
  endpoint instead reads the set once, assigns every new card's `uid`
  in-memory, and writes back once.
- **Export needs no request body/JSON at all** - the client just calls the
  endpoint and gets the finished CSV text directly, then triggers a
  browser file download (`Blob` + a temporary `<a download>` click) with
  whatever text came back. No client-side CSV building.
- **Import needs no client-side parsing either** - the picked file's raw
  text is sent as-is in the request body; the server parses it, validates
  it, and returns the updated `FlashcardsSet` so the Edit screen can
  refresh its list from the response directly.

A row with an empty Front or Back is silently skipped (not treated as an
error) - a stray blank line in an exported/hand-edited CSV shouldn't block
importing everything else. A completely malformed header (missing a
`Front` or `Back` column) fails the whole import with a clear error,
rather than silently importing garbage.

## Navigation

Home screen gets a new "Flashcards" tool entry, like YT Shadowing. Tapping
it shows a set picker - just a list of set names, in creation order.
Deleting a set asks for confirmation first (a reusable `ConfirmDialog`
component); deleting a single card doesn't.

There's no separate per-set detail page - tapping a set name opens Edit
mode directly, which doubles as the "view set" screen: title, a computed
`SetSummary` (see "API" above) shown as `<learned>/<total> learned`, and a
row of mode buttons - **Learn (count) / Review (count) / Practice**. Learn
and Review are disabled whenever there's nothing to do. Learn's count is
capped at the batch size constant (15) even if more cards are still
unlearned, since a Learning session only ever pulls one batch at a time;
Review's count is always the true total due count, uncapped, even though a
session still fetches due cards 15 at a time internally.

Cards are created manually only in v1 (front/back always typed by hand) -
dictionary-driven card creation (e.g. an "add to flashcards" button inside
the Dictionary popup) is future work. Sets can be deleted but not renamed,
and cards can't move between sets.

## Learn/Review/Practice card UI

Both sessions show the current card as one flashcard that fills the full
screen below the header (not the app's usual centered-column layout), and
keep all four grade buttons visible inside the card at all times - not
gated behind flipping - as an overlay pinned to the card's bottom edge.

Above the card, a row of small pill-shaped dots - one per card in the
current batch - tracks progress:

- **Order mirrors the actual live queue**, not a fixed session-start order:
  a card sent back of the pool (Learning's Hard/first Good, Review's
  Hard-while-recovering) moves toward the end of the row; a card reinserted
  via Again moves 5 dots ahead. Watching the row shuffle is a direct
  reflection of `FlashcardsLearningEngine`/`FlashcardsReviewEngine`'s
  internal queue reordering.
- **Completed cards occupy a fixed prefix at the front of the row**, in the
  order they finished, and never move again once there - a card that's
  actually done (graduated in Learning, passed in Review) stops
  participating in the reordering above. Each gets a thin grey ring to mark
  it as done.
- The **current card's dot gets a thicker white ring**, wherever it sits in
  the row.
- A dot is grey until its card has been graded at least once, then colored
  to match whichever grade button was last pressed for it (red/orange/
  green/blue for Again/Hard/Good/Easy) - a card graded Again then later
  passed with Good updates from red to green.

Both of the card's 3D-flip faces (front and back, in Learn, Review, and
Practice alike) are internally split into a top and bottom half by a
divider line, deliberately the same shape on both faces even though it's
redundant on the front - kept simple for now, may be revisited later:

- **Top half**: the front text, always - shown identically whether the
  card is flipped or not.
- **Bottom half**: on the front face (not yet flipped), a decorative
  blurred "?????" placeholder that reveals nothing about the real answer;
  on the back face (flipped), the actual back text.

Flipping (tapping the card, same gesture as always) swaps between these
two faces exactly as before - it's the *content* of each face that's new,
not the flip mechanic itself.

Each half has its own small Play/Record/Shadow button trio at its end
(`src/composables/useFlashcardFaceAudio.js`, one independent instance for
front's text and one for back's - the front-text trio is the *same*
instance whether it's rendered on the front face or repeated atop the back
face, so its mic session stays consistent regardless of flip state). Front
and back each get their own mic session, entirely independent of each
other; nothing is shown under the blurred placeholder, since there's
nothing to play/record/shadow yet. Play speaks that half's full text as
one phrase (not per-word) via plain TTS, deliberately **not** the
dictionary API's pronunciation-clip lookup (`playWordPronunciation` in
`wordAudioPlayer.js`) - a flashcard face is often a whole phrase, not a
single dictionary word, so there's no real pronunciation clip to prefer in
the first place. Record and Shadow reuse the exact same infrastructure as
everywhere else in the app (`useRecordShadow.js`/`RecordShadowButtons.vue`,
also used by YT Shadowing and the Dictionary popup) - Shadow plays the
text aloud, records for that duration + 0.25s, then plays the recording
back.

Play/Shadow always speak the text with its IPA/parenthetical annotations
stripped out first (see "IPA phonetic notation and parenthetical
annotations" below) - "record (noun)" is spoken as just "record". If a
half's text is *entirely* annotation (e.g. a pure-IPA back side,
"/ˈhɪs.t̬ɚ.i/"), there's nothing left to say - that half's whole
Play/Record/Shadow trio is hidden rather than acting on an empty phrase.

### IPA phonetic notation and parenthetical annotations

Front/back text is scanned for two kinds of annotation, each given its own
display treatment and neither individually word-clickable for a dictionary
lookup (unlike the rest of the text) - `/rɪˈpɔːr.t̬ɪd.li/` isn't a real
word to look up, and `(noun)` is a note about the word, not the word
itself:

- **`/.../` phonetic transcriptions** (the convention
  `scripts/cambridge_us_phonemes.py`'s CSV output already uses, including
  multiple transcripts separated by " || ") - loosely based on Cambridge
  Dictionary's own IPA styling (smaller, normal weight against the
  surrounding bold flashcard text), with added italic and more size
  contrast on top since Cambridge's exact subtlety (their real difference
  is just 14px vs 16px, no italic/color/font change at all) didn't read
  clearly at this app's much larger flashcard-text scale.
- **`(...)` parenthetical annotations** (e.g. "record (noun)") - italic and
  a dimmer color (`var(--text-dim)`), no size change.

Both kinds can appear any number of times in any order, mixed freely with
plain text and with each other - `src/engine/flashcardAnnotationSegmenter.js`
does the splitting with a single combined regex pass (a pure function,
plain unit-tested with cases covering arbitrary interleaving, not just the
simple text-then-annotation case); `src/components/FlashcardText.vue`
combines it with the existing per-word clickability and is what all three
session screens render front/back through now, instead of each duplicating
the tokenization template inline.

## Simulating day-by-day testing (dev-only)

Review's scheduling is inherently hard to test manually since it depends on
real calendar days passing. `src/engine/flashcardsToday.js` has a manual
escape hatch for this: set `VITE_TEST_FLASHCARDS_REFRESH_DAY=true` in a
gitignored `.env.local` (see `.gitignore`'s `*.local` rule) and rebuild.
With the flag on, `today()` stops returning the real date - instead it
reads a fake date out of `localStorage` (key `test-fake-today`), advances
it by exactly one calendar day, and writes it back, so **every page load
simulates the next day**. This only advances once per page load (cached in
a module-level variable, not recomputed on every `today()` call within the
same session) and seeds itself from the real date the first time nothing's
stored yet. Deliberately a `localStorage`-only hack, not routed through
`StorageMap`/`docs/local-storage.md`'s per-device settings system, since
it's throwaway dev tooling, not a real app setting - remove the `.env.local`
line and rebuild to go back to real dates.
