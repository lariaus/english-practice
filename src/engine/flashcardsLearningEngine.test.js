import { describe, expect, it } from 'vitest'
import { FlashcardsLearningEngine } from './flashcardsLearningEngine.js'

describe('FlashcardsLearningEngine', () => {
  it('learns a card after two Goods in a row', () => {
    const engine = new FlashcardsLearningEngine([1])
    engine.grade('GOOD')
    expect(engine.isDone).toBe(false)
    engine.grade('GOOD')
    expect(engine.isDone).toBe(true)
    expect(engine.learnedCards).toEqual([{ uid: 1, grade: 'GOOD' }])
  })

  it('two Goods with something else in between still learns on the second Good', () => {
    const engine = new FlashcardsLearningEngine([1, 2])
    engine.grade('GOOD') // card 1: streak 1, back of pool -> queue is [2, 1]
    engine.grade('HARD') // card 2: back of pool -> queue is [1, 2]
    engine.grade('GOOD') // card 1: streak 2 -> learned
    expect(engine.learnedCards).toEqual([{ uid: 1, grade: 'GOOD' }])
  })

  it('Again resets an in-progress streak back to 0', () => {
    const engine = new FlashcardsLearningEngine([1])
    engine.grade('GOOD') // streak -> 1
    engine.grade('AGAIN') // streak -> 0
    // If the reset hadn't happened, this next Good would land on a stale
    // streak of 1 and learn the card immediately - it shouldn't yet.
    engine.grade('GOOD') // streak -> 1
    expect(engine.isDone).toBe(false)
    engine.grade('GOOD') // streak -> 2
    expect(engine.learnedCards).toEqual([{ uid: 1, grade: 'GOOD' }])
  })

  it('Again reinserts a card 5 cards later, not immediately or at the very back', () => {
    const engine = new FlashcardsLearningEngine([1, 2, 3, 4, 5, 6, 7])
    engine.grade('AGAIN') // card 1 reinserted 5 later among the remaining 6 -> [2,3,4,5,6,1,7]
    expect(engine.currentUid).toBe(2)
    for (let i = 0; i < 4; i++) engine.grade('HARD') // pass 2,3,4,5 through, each to the back of the pool
    expect(engine.currentUid).toBe(6) // right before the reinserted card 1
    engine.grade('HARD')
    expect(engine.currentUid).toBe(1) // card 1 comes up next, exactly 5 cards after it failed
  })

  it('Easy learns a card immediately even at streak 0', () => {
    const engine = new FlashcardsLearningEngine([1])
    engine.grade('EASY')
    expect(engine.isDone).toBe(true)
    expect(engine.learnedCards).toEqual([{ uid: 1, grade: 'EASY' }])
  })

  it('exiting mid-batch only reports cards that actually reached learned', () => {
    const engine = new FlashcardsLearningEngine([1, 2])
    engine.grade('GOOD') // card 1 at streak 1 - never learned
    engine.grade('EASY') // card 2 - learned
    expect(engine.learnedCards).toEqual([{ uid: 2, grade: 'EASY' }])
  })
})
