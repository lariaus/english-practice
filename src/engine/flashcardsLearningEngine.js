// Learning's custom "good streak" mechanic - deliberately not Anki's real
// timed learning-steps model, see docs/flashcards-spec.md's "Learning"
// section for the full table this implements. Purely in-memory, no Vue, no
// network - the screen wraps this in reactive state and calls
// flashcardsClient.markCardsLearned() once with whatever learnedCards
// reports, on completion or early exit.

const AGAIN_REINSERT_OFFSET = 5
const STREAK_TO_LEARN = 2

export class FlashcardsLearningEngine {
  constructor(uids) {
    this._queue = uids.map((uid) => ({ uid, streak: 0 }))
    this._learned = []
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

  // list[{uid, grade: 'GOOD'|'EASY'}] - only cards that have actually
  // reached "learned" so far, ready to hand to markCardsLearned().
  get learnedCards() {
    return this._learned
  }

  // button: 'AGAIN' | 'HARD' | 'GOOD' | 'EASY'
  grade(button) {
    if (this._queue.length === 0) return
    const entry = this._queue.shift()

    if (button === 'EASY') {
      this._learned.push({ uid: entry.uid, grade: 'EASY' })
      return
    }

    if (button === 'AGAIN') {
      entry.streak = 0
      const reinsertAt = Math.min(AGAIN_REINSERT_OFFSET, this._queue.length)
      this._queue.splice(reinsertAt, 0, entry)
      return
    }

    if (button === 'GOOD') {
      entry.streak += 1
      if (entry.streak >= STREAK_TO_LEARN) {
        this._learned.push({ uid: entry.uid, grade: 'GOOD' })
        return
      }
      this._queue.push(entry) // back of the pool - unchanged position, just not advanced
      return
    }

    // HARD: streak unchanged, back of the pool.
    this._queue.push(entry)
  }
}
