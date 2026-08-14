import { describe, expect, it } from 'vitest'
import { FlashcardsReviewEngine } from './flashcardsReviewEngine.js'

describe('FlashcardsReviewEngine', () => {
  it('Good on a first attempt completes with lapsed: false', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('GOOD')
    expect(engine.isDone).toBe(true)
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: false }])
  })

  it('a lapsed card needs two Goods in a row to actually complete, not one', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('AGAIN')
    engine.grade('HARD') // can't complete a recovery - reinserted again
    expect(engine.isDone).toBe(false)
    engine.grade('GOOD') // streak -> 1, not enough yet
    expect(engine.isDone).toBe(false)
    expect(engine.results).toEqual([])
    engine.grade('GOOD') // streak -> 2, completes
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: true }])
  })

  it('Hard while recovering resets the Good streak back to 0, not just failing to advance it', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('AGAIN')
    engine.grade('GOOD') // streak -> 1
    engine.grade('HARD') // streak -> 0
    engine.grade('GOOD') // streak -> 1 again (if Hard hadn't reset it, this would complete)
    expect(engine.isDone).toBe(false)
    engine.grade('GOOD') // streak -> 2, completes
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: true }])
  })

  it('Easy bypasses the two-Good streak entirely, completing a lapsed card immediately', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('AGAIN')
    engine.grade('EASY') // no streak built up at all, still completes right away
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'EASY', lapsed: true }])
  })

  it('Again, Again, Easy produces one compact result, not three', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('AGAIN')
    engine.grade('AGAIN')
    engine.grade('EASY')
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'EASY', lapsed: true }])
  })

  it('a card still mid-retry when the session ends is simply absent from results', () => {
    const engine = new FlashcardsReviewEngine([1, 2])
    engine.grade('GOOD') // card 1 completes
    engine.grade('AGAIN') // card 2 still retrying, never completes this "session"
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: false }])
  })

  it('Again reinserts 5 cards later, matching Learning’s scheme', () => {
    const engine = new FlashcardsReviewEngine([1, 2, 3, 4, 5, 6, 7])
    engine.grade('AGAIN') // card 1 reinserted 5 later among the remaining 6 -> [2,3,4,5,6,1,7]
    expect(engine.currentUid).toBe(2)
    for (let i = 0; i < 4; i++) engine.grade('GOOD') // 2,3,4,5 all complete and leave the queue
    expect(engine.currentUid).toBe(6)
    engine.grade('GOOD')
    expect(engine.currentUid).toBe(1)
  })

  it('a Good that does not yet complete a lapsed card goes to the very back of the queue, not 5 cards later', () => {
    const engine = new FlashcardsReviewEngine([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
    engine.grade('AGAIN') // card 1 lapses, reinserted 5 later among the remaining 11 -> [2,3,4,5,6,1,7,8,9,10,11,12]
    for (let i = 0; i < 5; i++) engine.grade('GOOD') // 2,3,4,5,6 all complete (first attempt) -> queue is [1,7,8,9,10,11,12]
    expect(engine.currentUid).toBe(1)
    engine.grade('GOOD') // card 1: streak -> 1, not complete - back of the queue, not 5-cards-later
    expect(engine.queueUids).toEqual([7, 8, 9, 10, 11, 12, 1])
  })

  it('Hard on a first attempt completes normally (does not lapse)', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('HARD')
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'HARD', lapsed: false }])
  })

  it('queueEntries reflects each card\'s current lapsed flag and streak, not just its uid', () => {
    const engine = new FlashcardsReviewEngine([1, 2])
    engine.grade('AGAIN') // card 1: lapsed -> true, streak stays 0, reinserted
    expect(engine.queueEntries).toEqual([
      { uid: 2, lapsed: false, streak: 0 },
      { uid: 1, lapsed: true, streak: 0 },
    ])
  })

  it('restore() reconstructs a mid-recovery session that behaves exactly like a fresh one', () => {
    const original = new FlashcardsReviewEngine([1, 2])
    original.grade('AGAIN') // card 1: lapsed -> true, reinserted -> queue is [2, 1]

    const restored = FlashcardsReviewEngine.restore(original.queueEntries, original.results)
    expect(restored.queueUids).toEqual(original.queueUids)
    expect(restored.results).toEqual(original.results)

    restored.grade('GOOD') // card 2 completes normally (first attempt, not lapsed)
    restored.grade('GOOD') // card 1: streak -> 1, not enough yet
    expect(restored.isDone).toBe(false)
    restored.grade('GOOD') // card 1: streak -> 2, completes its recovery -> lapsed: true
    expect(restored.results).toEqual([
      { card_uid: 2, grade: 'GOOD', lapsed: false },
      { card_uid: 1, grade: 'GOOD', lapsed: true },
    ])
  })

  it('restore() produces an independent copy, not a reference to the saved arrays', () => {
    const savedQueue = [{ uid: 1, lapsed: true, streak: 1 }]
    const savedResults = [{ card_uid: 2, grade: 'GOOD', lapsed: false }]
    const restored = FlashcardsReviewEngine.restore(savedQueue, savedResults)
    restored.grade('GOOD') // streak -> 2, completes - would mutate a shared array in place if not copied
    expect(savedQueue).toEqual([{ uid: 1, lapsed: true, streak: 1 }])
    expect(savedResults).toEqual([{ card_uid: 2, grade: 'GOOD', lapsed: false }])
  })

  it('restore() defaults a missing streak to 0, for a session saved before this field existed', () => {
    const restored = FlashcardsReviewEngine.restore([{ uid: 1, lapsed: true }], [])
    restored.grade('GOOD') // streak -> 1 (not NaN), not enough yet
    expect(restored.isDone).toBe(false)
    restored.grade('GOOD') // streak -> 2, completes
    expect(restored.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: true }])
  })
})
