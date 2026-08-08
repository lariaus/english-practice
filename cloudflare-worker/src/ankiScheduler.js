// Legacy Anki-style scheduling math - see docs/anki-algorithm.md for the
// exact constants/formulas this implements, and docs/flashcards-spec.md's
// "Review" section for how they're actually applied (the due-anchored-to-
// today rule, the once-per-lapse-episode penalty, interval rounding, no
// interval cap). Pure functions only, no KV/HTTP - covered by plain unit
// tests, no Miniflare needed for this part.

export const STARTING_EASE = 2.5
export const EASE_FLOOR = 1.3
export const GRADUATING_INTERVAL_DAYS = 1
export const EASY_GRADUATING_INTERVAL_DAYS = 4
export const LAPSE_INTERVAL_DAYS = 1

const HARD_EASE_DELTA = -0.15
const EASY_EASE_DELTA = 0.15
const LAPSE_EASE_PENALTY = 0.2
const HARD_INTERVAL_MULTIPLIER = 1.2
const EASY_INTERVAL_BONUS = 1.3

function addDays(dateStr, days) {
  const date = new Date(`${dateStr}T00:00:00Z`)
  date.setUTCDate(date.getUTCDate() + days)
  return date.toISOString().slice(0, 10)
}

// Avoids floating-point drift accumulating across many ease adjustments
// (0.15/0.20 deltas aren't exactly representable in binary floating point).
function roundEase(ease) {
  return Math.round(ease * 100) / 100
}

// Graduating a card out of Learning into Review - see
// docs/flashcards-spec.md's Learning section. Fixed defaults; Learning
// never sends any per-card scheduling data, just which grade graduated it.
export function graduateCard(grade, today) {
  const interval_days = grade === 'EASY' ? EASY_GRADUATING_INTERVAL_DAYS : GRADUATING_INTERVAL_DAYS
  return {
    state: 'REVIEW',
    ease_factor: STARTING_EASE,
    interval_days,
    due: addDays(today, interval_days),
  }
}

// Applies one ReviewResult ({grade, lapsed}) to a card already in Review -
// see docs/flashcards-spec.md's Review section for the full table this
// implements. `card` only needs `ease_factor`/`interval_days`/`lapses`.
// `due` is always anchored to `today` (the actual review date), never to
// the card's old `due` - see "Open questions - round 13" in the spec.
export function gradeReview(card, { grade, lapsed }, today) {
  if (lapsed) {
    const ease_factor = Math.max(EASE_FLOOR, roundEase(card.ease_factor - LAPSE_EASE_PENALTY))
    const interval_days = LAPSE_INTERVAL_DAYS
    return {
      state: 'REVIEW',
      ease_factor,
      interval_days,
      due: addDays(today, interval_days),
      lapses: card.lapses + 1,
    }
  }

  const oldEase = card.ease_factor
  let ease_factor = oldEase
  let interval_days

  if (grade === 'HARD') {
    ease_factor = Math.max(EASE_FLOOR, roundEase(oldEase + HARD_EASE_DELTA))
    interval_days = Math.round(card.interval_days * HARD_INTERVAL_MULTIPLIER)
  } else if (grade === 'GOOD') {
    interval_days = Math.round(card.interval_days * oldEase)
  } else if (grade === 'EASY') {
    ease_factor = roundEase(oldEase + EASY_EASE_DELTA)
    interval_days = Math.round(card.interval_days * oldEase * EASY_INTERVAL_BONUS)
  }

  return {
    state: 'REVIEW',
    ease_factor,
    interval_days,
    due: addDays(today, interval_days),
    lapses: card.lapses,
  }
}
