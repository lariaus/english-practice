import { describe, expect, it } from 'vitest'
import { parseCsv, serializeCsv } from '../src/csvFlashcards.js'

describe('parseCsv', () => {
  it('parses the basic example format, including a trailing empty column', () => {
    const csv = 'Front,Back,\nBleu,Blue,\nRouge,Red,\n'
    expect(parseCsv(csv)).toEqual([
      { front: 'Bleu', back: 'Blue' },
      { front: 'Rouge', back: 'Red' },
    ])
  })

  it('matches header columns case-insensitively and in any order', () => {
    const csv = 'BACK,front\nBlue,Bleu\n'
    expect(parseCsv(csv)).toEqual([{ front: 'Bleu', back: 'Blue' }])
  })

  it('handles a quoted field containing a comma', () => {
    const csv = 'Front,Back\n"Bonjour, comment ça va?",Hello there\n'
    expect(parseCsv(csv)).toEqual([{ front: 'Bonjour, comment ça va?', back: 'Hello there' }])
  })

  it('handles a quoted field containing an escaped double quote', () => {
    const csv = 'Front,Back\n"She said ""hi""",Greeting\n'
    expect(parseCsv(csv)).toEqual([{ front: 'She said "hi"', back: 'Greeting' }])
  })

  it('handles a quoted field containing an embedded newline', () => {
    const csv = 'Front,Back\n"Line one\nLine two",Multi-line front\n'
    expect(parseCsv(csv)).toEqual([{ front: 'Line one\nLine two', back: 'Multi-line front' }])
  })

  it('skips a row with an empty front or back rather than failing the whole import', () => {
    const csv = 'Front,Back\nBleu,Blue\n,Missing front\nRouge,\nVert,Green\n'
    expect(parseCsv(csv)).toEqual([
      { front: 'Bleu', back: 'Blue' },
      { front: 'Vert', back: 'Green' },
    ])
  })

  it('preserves UTF-8 content (French accents, IPA)', () => {
    const csv = 'Front,Back\ndéjà vu,/deɪʒɑː vuː/\n'
    expect(parseCsv(csv)).toEqual([{ front: 'déjà vu', back: '/deɪʒɑː vuː/' }])
  })

  it('throws a clear error when the header is missing Front/Back columns', () => {
    const csv = 'Question,Answer\nBleu,Blue\n'
    expect(() => parseCsv(csv)).toThrow(/Front.*Back/)
  })

  it('throws on a completely empty file', () => {
    expect(() => parseCsv('')).toThrow('CSV is empty')
  })
})

describe('serializeCsv', () => {
  it('serializes plain cards with a header row', () => {
    const cards = [
      { front: 'Bleu', back: 'Blue', uid: 1 },
      { front: 'Rouge', back: 'Red', uid: 2 },
    ]
    expect(serializeCsv(cards)).toBe('Front,Back\r\nBleu,Blue\r\nRouge,Red\r\n')
  })

  it('quotes a field containing a comma', () => {
    expect(serializeCsv([{ front: 'Bonjour, ça va?', back: 'Hello' }])).toBe(
      'Front,Back\r\n"Bonjour, ça va?",Hello\r\n',
    )
  })

  it('quotes and escapes a field containing a double quote', () => {
    expect(serializeCsv([{ front: 'She said "hi"', back: 'Greeting' }])).toBe(
      'Front,Back\r\n"She said ""hi""",Greeting\r\n',
    )
  })

  it('round-trips through parseCsv exactly', () => {
    const cards = [
      { front: 'déjà vu, château', back: '/deɪʒɑː vuː/' },
      { front: 'She said "hi"', back: 'Line one\nLine two' },
    ]
    const csv = serializeCsv(cards)
    expect(parseCsv(csv)).toEqual(cards)
  })
})
