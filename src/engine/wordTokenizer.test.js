import { describe, expect, it } from 'vitest'
import { cleanWord, isClickableWord, splitIntoWords } from './wordTokenizer.js'

describe('splitIntoWords', () => {
  it('splits on whitespace, preserving the separators as their own tokens', () => {
    expect(splitIntoWords('voiture rouge')).toEqual(['voiture', ' ', 'rouge'])
  })
})

describe('isClickableWord', () => {
  it('treats accented letters as word-like, not punctuation', () => {
    expect(isClickableWord('café')).toBe(true)
    expect(isClickableWord('garçon')).toBe(true)
  })

  it('treats IPA characters as word-like', () => {
    expect(isClickableWord('ʃæt.oʊ')).toBe(true)
  })

  it('treats a pure-whitespace token as not clickable', () => {
    expect(isClickableWord(' ')).toBe(false)
  })

  it('treats a pure-punctuation token as not clickable', () => {
    expect(isClickableWord('.,!?')).toBe(false)
  })
})

describe('cleanWord', () => {
  it('strips surrounding punctuation, keeping the word intact', () => {
    expect(cleanWord('"Bonjour,')).toBe('Bonjour')
  })

  it('does not mangle accented letters at a word boundary - the actual bug being fixed', () => {
    // \w is ASCII-only ([A-Za-z0-9_]) - a naive /^[^\w']+|[^\w']+$/g cleanup
    // would treat the trailing "é" as punctuation and strip it, turning
    // "café" into "caf". This is exactly the French-vocabulary case
    // Flashcards needs to get right.
    expect(cleanWord('café')).toBe('café')
    expect(cleanWord('garçon.')).toBe('garçon')
    expect(cleanWord('(château)')).toBe('château')
  })

  it('keeps an internal apostrophe intact', () => {
    expect(cleanWord("l'arbre")).toBe("l'arbre")
  })

  it('returns an empty string for a token with no word content', () => {
    expect(cleanWord('---')).toBe('')
  })
})
