// Review's retry-until-passed queue - see docs/flashcards-spec.md's
// "Review" section for the full first-attempt-vs-recovering table this
// implements. Purely in-memory, no Vue, no network - the screen wraps this
// in reactive state and calls flashcardsClient.sendReviewResults() once
// with whatever results reports, on completion or early exit.

const REINSERT_OFFSET = 5
const STREAK_TO_RECOVER = 2

export class FlashcardsReviewEngine {
  constructor(uids) {
    this._queue = uids.map((uid) => ({ uid, lapsed: false, streak: 0 }))
    this._results = []
  }

  // Reconstructs an engine from a previously saved queue/results snapshot
  // (see flashcardsSessionProgress.js) instead of a fresh uid list - used
  // to resume a session across an app restart. Behaves identically to an
  // engine that reached the same state by grading through from scratch.
  static restore(queue, results) {
    const engine = new FlashcardsReviewEngine([])
    engine._queue = queue.map((entry) => ({ ...entry }))
    engine._results = results.map((entry) => ({ ...entry }))
    return engine
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

  // Raw queue entries (uid + lapsed + streak), for persisting a resumable
  // snapshot - queueUids above only exposes the uid, not enough to resume
  // mid-recovery.
  get queueEntries() {
    return this._queue.map((entry) => ({ ...entry }))
  }

  // list[{card_uid, grade: 'HARD'|'GOOD'|'EASY', lapsed}] - only cards
  // that have actually completed (passed) so far, ready to hand to
  // sendReviewResults(). Never contains a raw AGAIN grade - see
  // docs/flashcards-spec.md.
  get results() {
    return this._results
  }

  // button: 'AGAIN' | 'HARD' | 'GOOD' | 'EASY'
  //
  // A card that's lapsed needs two consecutive Goods to actually complete
  // (see docs/flashcards-spec.md's "Review" section) - a deliberate
  // deviation from real Anki's single relearning step, mirroring Learning's
  // own streak-of-2 requirement. Easy always bypasses this and completes on
  // the spot, same as Easy already does in Learning; a first attempt (never
  // lapsed) is unaffected either way - only recovery requires the streak.
  grade(button) {
    if (this._queue.length === 0) return
    const entry = this._queue.shift()
    const recovering = entry.lapsed

    if (button === 'AGAIN') {
      entry.lapsed = true
      entry.streak = 0
      this._reinsert(entry)
      return
    }

    if (button === 'HARD') {
      if (recovering) {
        // Hard can't make progress toward completing a recovery, and wipes
        // out whatever streak was already built up - same severity as
        // Again for this purpose (see docs/anki-algorithm.md for why Hard
        // can't complete a recovery at all).
        entry.streak = 0
        this._reinsert(entry)
        return
      }
      this._results.push({ card_uid: entry.uid, grade: 'HARD', lapsed: false })
      return
    }

    if (button === 'EASY') {
      this._results.push({ card_uid: entry.uid, grade: 'EASY', lapsed: recovering })
      return
    }

    // GOOD
    if (recovering) {
      entry.streak = (entry.streak ?? 0) + 1 // ?? 0 covers a resumed session saved before this field existed
      if (entry.streak >= STREAK_TO_RECOVER) {
        this._results.push({ card_uid: entry.uid, grade: 'GOOD', lapsed: true })
        return
      }
      // Got it right, just not enough times yet - back of the queue, not
      // the urgent 5-cards-later reinsert (that's for actually getting it
      // wrong again - see AGAIN/HARD above). Matches
      // FlashcardsLearningEngine's identical distinction: a Good that
      // doesn't yet graduate also goes to the back of the pool, never the
      // reinsert-offset treatment.
      this._queue.push(entry)
      return
    }

    this._results.push({ card_uid: entry.uid, grade: 'GOOD', lapsed: false })
  }

  _reinsert(entry) {
    const reinsertAt = Math.min(REINSERT_OFFSET, this._queue.length)
    this._queue.splice(reinsertAt, 0, entry)
  }
}
