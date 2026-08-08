// Review's retry-until-passed queue - see docs/flashcards-spec.md's
// "Review" section for the full first-attempt-vs-recovering table this
// implements. Purely in-memory, no Vue, no network - the screen wraps this
// in reactive state and calls flashcardsClient.sendReviewResults() once
// with whatever results reports, on completion or early exit.

const REINSERT_OFFSET = 5

export class FlashcardsReviewEngine {
  constructor(uids) {
    this._queue = uids.map((uid) => ({ uid, lapsed: false }))
    this._results = []
  }

  get currentUid() {
    return this._queue.length > 0 ? this._queue[0].uid : null
  }

  get isDone() {
    return this._queue.length === 0
  }

  get remainingCount() {
    return this._queue.length
  }

  // Current live queue order - e.g. after grading Good on the front card of
  // [foo, bar, baz], this returns [bar, baz, foo]. Drives the progress dots,
  // which must stay in sync with the actual queue rather than a fixed slot.
  get queueUids() {
    return this._queue.map((entry) => entry.uid)
  }

  // list[{card_uid, grade: 'HARD'|'GOOD'|'EASY', lapsed}] - only cards
  // that have actually completed (passed) so far, ready to hand to
  // sendReviewResults(). Never contains a raw AGAIN grade - see
  // docs/flashcards-spec.md.
  get results() {
    return this._results
  }

  // button: 'AGAIN' | 'HARD' | 'GOOD' | 'EASY'
  grade(button) {
    if (this._queue.length === 0) return
    const entry = this._queue.shift()
    const recovering = entry.lapsed

    if (button === 'AGAIN') {
      entry.lapsed = true
      this._reinsert(entry)
      return
    }

    if (button === 'HARD' && recovering) {
      // Hard can't complete a relearning recovery - behaves like Again
      // (see docs/anki-algorithm.md).
      this._reinsert(entry)
      return
    }

    // HARD on a first attempt, or GOOD/EASY either way: completes.
    this._results.push({ card_uid: entry.uid, grade: button, lapsed: recovering })
  }

  _reinsert(entry) {
    const reinsertAt = Math.min(REINSERT_OFFSET, this._queue.length)
    this._queue.splice(reinsertAt, 0, entry)
  }
}
