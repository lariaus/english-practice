# Anki's legacy scheduling algorithm (reference)

The exact default constants and mechanics of Anki's **legacy, SM-2-based**
scheduler (not FSRS, Anki's newer default since 23.10 - Flashcards
deliberately uses the legacy one, see `docs/flashcards-spec.md`'s "Review
(spaced repetition)" section). Written as a standalone reference since
this is self-contained knowledge, reusable by anything that needs it -
Flashcards is the first consumer, not necessarily the only one.

Confirmed via a dedicated research pass cross-checking Anki's official
manual/FAQ prose against the actual Anki source
(`ankitects/anki`, `rslib/src/deckconfig/`) - noted below wherever the two
disagreed.

## Card states

A card is always in exactly one of:

- **New** - never studied.
- **Learning** - actively working through the learning steps (below), not
  yet on a daily interval.
- **Review** - graduated; scheduled by day-granularity intervals.
- **Relearning** - fell out of Review after a lapse (an "Again" during
  Review), working through the relearning steps before returning to Review.

## Learning (New → Review)

- **Learning steps: `1m, 10m`** (two steps, in minutes).
- **Good** advances to the next step; on the *last* step, Good graduates the
  card into Review with interval = **graduating interval, 1 day**.
- **Again** sends the card back to the *first* learning step, regardless of
  which step it was on.
- **Easy** graduates the card into Review immediately, from *any* step (not
  just the last one), with interval = **easy interval, 4 days**.
- **Hard** is not a distinct case during learning in the legacy scheduler -
  it behaves like Again/repeats the current step timing-wise (not a
  separate ease/interval mechanic - that only exists once in Review).

## Review

Starting ease factor for every card: **2.50 (250%)**. Per button, once a
card is in Review:

| Button | Ease change | Interval multiplier |
|---|---|---|
| Again | −20 points | → card leaves Review, enters Relearning (see below) |
| Hard | −15 points | × **1.2** (hard interval) |
| Good | unchanged | × current ease factor |
| Easy | +15 points | × current ease factor × **1.3** (easy bonus) |

- All of Hard/Good/Easy's computed intervals are further multiplied by the
  **interval modifier**, default **1.00** (a no-op at default settings, but
  a single global multiplier if ever tuned).
- **Ease floor: 130% (1.3)** - ease is never allowed to drop below this,
  regardless of how many Agains/Hards accumulate.
- A small **fuzz** (randomization) is applied to computed review intervals,
  specifically to stop cards rated together from clumping onto the same
  future day. See "Fuzz, exactly" below for the precise formula. Learning-
  phase timing gets its own small random delay (up to ~5 minutes) for
  ordering purposes, separate from review-interval fuzz.

## Fuzz, exactly

**Status: not implemented.** Documented here in full for reference/future
use, but v1 deliberately skips it - meaningfully more implementation
complexity than the entire rest of the scheduling algorithm combined, for a
benefit (smoothing review-load spikes) that mostly only shows up at a
card-count/time scale this app likely won't reach. All computed intervals
in v1 are the plain deterministic numbers from the tables above, un-fuzzed.
Revisit if review-load clumping ever actually becomes noticeable in
practice.

Confirmed from Anki's current Rust source
(`rslib/src/scheduler/states/fuzz.rs`) - two implementations exist
historically, this is the modern one, actually shipping today:

- **Ranges**: 2.5-7 days → ±15%, 7-20 days → ±10%, 20+ days → ±5%. This
  differs from the "±25/15/5%" numbers some older blog posts/guides cite -
  those describe Anki's *old, pre-Rust-rewrite* Python scheduler
  (`pylib/anki/sched.py`), not what current Anki actually runs.
- **The accumulation formula** (`fuzz_delta(interval)`), for an interval
  spanning multiple ranges: start from a base of 1.0 (one day), then for
  each range the interval reaches, add `factor × (min(interval, range_end) −
  range_start)`. Worked example for interval = 37 days: `1 + 0.15×(7−2.5) +
  0.10×(20−7) + 0.05×(37−20) = 1 + 0.675 + 1.3 + 0.85 = 4.325` days of
  delta, giving bounds of roughly `[32.7, 41.3]`.
- **Final value**: `lower = round(interval − delta)`, `upper =
  round(interval + delta)`, then `final = floor(lower + fuzz_factor × (1 +
  upper − lower))`, where `fuzz_factor` is drawn uniformly from `[0.0,
  1.0)`. This is a **uniform distribution** across the `[lower, upper]`
  range, inclusive.
- **Applies broadly**: graduating intervals, the post-lapse minimum
  interval, and Hard/Good/Easy review intervals - not just the ease-
  multiplier step.
- **No fuzz below 2.5 days** - `fuzz_delta` returns 0 in that case, so a
  1-day interval (the default graduating interval) is never fuzzed.
- **Deterministic in real Anki**: seeded by `card_id + review_count`
  (`get_fuzz_seed_for_id_and_reps`), not truly random - the same card at
  the same rep count always fuzzes identically, which matters for Anki's
  own reproducibility/testing. A from-scratch implementation could use
  genuine randomness instead if reproducibility isn't a goal - bit-for-bit
  identical output to real Anki isn't achievable either way without using
  the exact same PRNG algorithm, not just "a" seeded RNG.

## Lapses → Relearning → back to Review

- Failing a Review card (Again) moves it to **Relearning**, working through
  **relearning steps: `10m`** (a single step).
- On completing relearning (Good), the card returns to Review. Its new
  interval is the *pre-lapse* interval × **"new interval," default 0.00**,
  then floored at the **minimum interval, default 1 day**. Concretely, at
  default settings: `max(old_interval × 0, 1 day) = 1 day` - not literally
  "0 days," despite the 0.00 multiplier.
- Ease was already dropped 20 points at the moment of the Again (per the
  Review table above) - relearning itself doesn't further change ease.

## Leeches

- **Threshold: 8 lapses** (Again-while-in-Review events, cumulative).
- **Default action: tag only** (`LeechAction::TagOnly`). This is a real
  point of confusion worth flagging explicitly: Anki's own manual prose
  ("Anki tags the note as a leech and suspends the card") reads as if
  suspending is the default, but the actual shipped source code's default
  (`schema11.rs`'s `LeechAction` enum, `#[default]` on `TagOnly`, confirmed
  against `rslib/src/deckconfig/mod.rs`'s `DEFAULT_DECK_CONFIG_INNER`) tags
  the card only - it keeps showing up for review normally, just flagged.
  `Suspend` is the *other*, non-default option, not what a fresh unmodified
  deck actually does.

## Sources

- <https://docs.ankiweb.net/deck-options.html>
- <https://docs.ankiweb.net/leeches.html>
- <https://docs.ankiweb.net/studying.html>
- <https://faqs.ankiweb.net/what-spaced-repetition-algorithm.html>
- <https://github.com/ankitects/anki/blob/main/rslib/src/deckconfig/mod.rs>
  (`DEFAULT_DECK_CONFIG_INNER` / `impl Default for DeckConfig`)
- <https://github.com/ankitects/anki/blob/main/rslib/src/deckconfig/schema11.rs>
  (`LeechAction::default()`)
- <https://github.com/ankitects/anki/blob/main/rslib/src/scheduler/states/fuzz.rs>
  (current fuzz ranges/logic)
- <https://github.com/ankitects/anki/blob/main/rslib/src/scheduler/answering/mod.rs>
  (`get_fuzz_seed_for_id_and_reps` - the deterministic seeding)
