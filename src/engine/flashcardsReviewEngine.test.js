import { describe, expect, it } from 'vitest'
import { FlashcardsReviewEngine } from './flashcardsReviewEngine.js'

describe('FlashcardsReviewEngine', () => {
  it('Good on a first attempt completes with lapsed: false', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('GOOD')
    expect(engine.isDone).toBe(true)
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: false }])
  })

  it('Again then Good completes with lapsed: true, never a Hard grade from a Hard press while recovering', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('AGAIN')
    expect(engine.isDone).toBe(false)
    engine.grade('HARD') // can't complete a recovery - reinserted again
    expect(engine.isDone).toBe(false)
    engine.grade('GOOD')
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'GOOD', lapsed: true }])
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

  it('Hard on a first attempt completes normally (does not lapse)', () => {
    const engine = new FlashcardsReviewEngine([1])
    engine.grade('HARD')
    expect(engine.results).toEqual([{ card_uid: 1, grade: 'HARD', lapsed: false }])
  })
})
