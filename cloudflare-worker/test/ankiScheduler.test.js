import { describe, expect, it } from 'vitest'
import { graduateCard, gradeReview } from '../src/ankiScheduler.js'

describe('graduateCard', () => {
  it('GOOD graduates to a 1-day interval at starting ease', () => {
    expect(graduateCard('GOOD', '2026-01-01')).toEqual({
      state: 'REVIEW',
      ease_factor: 2.5,
      interval_days: 1,
      due: '2026-01-02',
    })
  })

  it('EASY graduates to a 4-day interval at starting ease', () => {
    expect(graduateCard('EASY', '2026-01-01')).toEqual({
      state: 'REVIEW',
      ease_factor: 2.5,
      interval_days: 4,
      due: '2026-01-05',
    })
  })

  it('crosses a month boundary correctly', () => {
    expect(graduateCard('EASY', '2026-01-30').due).toBe('2026-02-03')
  })

  it('crosses a year boundary correctly', () => {
    expect(graduateCard('EASY', '2025-12-29').due).toBe('2026-01-02')
  })
})

describe('gradeReview - not lapsed', () => {
  const card = { ease_factor: 2.5, interval_days: 3, lapses: 0 }

  it('GOOD leaves ease unchanged, interval = interval × ease, rounded', () => {
    const result = gradeReview(card, { grade: 'GOOD', lapsed: false }, '2026-01-01')
    expect(result.ease_factor).toBe(2.5)
    expect(result.interval_days).toBe(8) // round(3 * 2.5) = round(7.5) = 8
    expect(result.due).toBe('2026-01-09')
    expect(result.lapses).toBe(0)
  })

  it('HARD drops ease 15 points, interval = interval × 1.2, rounded', () => {
    const result = gradeReview(card, { grade: 'HARD', lapsed: false }, '2026-01-01')
    expect(result.ease_factor).toBe(2.35)
    expect(result.interval_days).toBe(4) // round(3 * 1.2) = round(3.6) = 4
    expect(result.due).toBe('2026-01-05')
  })

  it('EASY raises ease 15 points, interval = interval × ease × 1.3, rounded', () => {
    const result = gradeReview(card, { grade: 'EASY', lapsed: false }, '2026-01-01')
    expect(result.ease_factor).toBe(2.65)
    expect(result.interval_days).toBe(10) // round(3 * 2.5 * 1.3) = round(9.75) = 10
    expect(result.due).toBe('2026-01-11')
  })

  it('HARD never drops ease below the 1.3 floor', () => {
    const result = gradeReview({ ease_factor: 1.35, interval_days: 5, lapses: 3 }, { grade: 'HARD', lapsed: false }, '2026-01-01')
    expect(result.ease_factor).toBe(1.3)
  })

  it('EASY has no ceiling on ease', () => {
    const result = gradeReview({ ease_factor: 3.5, interval_days: 5, lapses: 0 }, { grade: 'EASY', lapsed: false }, '2026-01-01')
    expect(result.ease_factor).toBe(3.65)
  })
})

describe('gradeReview - lapsed', () => {
  it('applies the ease penalty, increments lapses, and resets to a flat 1-day interval', () => {
    const card = { ease_factor: 2.5, interval_days: 30, lapses: 2 }
    const result = gradeReview(card, { grade: 'GOOD', lapsed: true }, '2026-01-01')
    expect(result.ease_factor).toBe(2.3)
    expect(result.interval_days).toBe(1)
    expect(result.due).toBe('2026-01-02')
    expect(result.lapses).toBe(3)
  })

  it('the passing grade (GOOD vs EASY) does not change the reset outcome', () => {
    const card = { ease_factor: 2.5, interval_days: 30, lapses: 2 }
    const good = gradeReview(card, { grade: 'GOOD', lapsed: true }, '2026-01-01')
    const easy = gradeReview(card, { grade: 'EASY', lapsed: true }, '2026-01-01')
    expect(easy).toEqual(good)
  })

  it('never drops ease below the 1.3 floor even when already close to it', () => {
    const card = { ease_factor: 1.35, interval_days: 10, lapses: 7 }
    const result = gradeReview(card, { grade: 'GOOD', lapsed: true }, '2026-01-01')
    expect(result.ease_factor).toBe(1.3)
  })
})
