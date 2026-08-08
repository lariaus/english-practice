import { describe, expect, it } from 'vitest'
import { splitIntoAnnotatedSegments, stripAnnotations } from './flashcardAnnotationSegmenter.js'

describe('splitIntoAnnotatedSegments', () => {
  it('returns a single text segment when there is no annotation', () => {
    expect(splitIntoAnnotatedSegments('hello')).toEqual([{ type: 'text', value: 'hello' }])
  })

  it('splits a single IPA transcription out of surrounding text', () => {
    expect(splitIntoAnnotatedSegments('hierarchy, /ˈhaɪ.rɑːr.ki/')).toEqual([
      { type: 'text', value: 'hierarchy, ' },
      { type: 'ipa', value: '/ˈhaɪ.rɑːr.ki/' },
    ])
  })

  it('returns just one ipa segment when the whole text is IPA', () => {
    expect(splitIntoAnnotatedSegments('/rɪˈpɔːr.t̬ɪd.li/')).toEqual([{ type: 'ipa', value: '/rɪˈpɔːr.t̬ɪd.li/' }])
  })

  it('handles multiple transcripts separated by plain text (the || convention)', () => {
    expect(splitIntoAnnotatedSegments('/əˈpɪn.jən/ || /əˈpɪn·jən/')).toEqual([
      { type: 'ipa', value: '/əˈpɪn.jən/' },
      { type: 'text', value: ' || ' },
      { type: 'ipa', value: '/əˈpɪn·jən/' },
    ])
  })

  it('treats a lone unmatched slash as plain text, not IPA', () => {
    expect(splitIntoAnnotatedSegments('a/b')).toEqual([{ type: 'text', value: 'a/b' }])
  })

  it('handles ipa + text + ipa', () => {
    expect(splitIntoAnnotatedSegments('/a/ mid /b/')).toEqual([
      { type: 'ipa', value: '/a/' },
      { type: 'text', value: ' mid ' },
      { type: 'ipa', value: '/b/' },
    ])
  })

  it('handles text + ipa + ipa + ipa, with no text between the ipa runs', () => {
    expect(splitIntoAnnotatedSegments('start /a//b//c/')).toEqual([
      { type: 'text', value: 'start ' },
      { type: 'ipa', value: '/a/' },
      { type: 'ipa', value: '/b/' },
      { type: 'ipa', value: '/c/' },
    ])
  })

  it('handles an arbitrary mix - ipa, text, ipa, ipa, text, ipa', () => {
    expect(splitIntoAnnotatedSegments('/a/x/b//c/y/d/')).toEqual([
      { type: 'ipa', value: '/a/' },
      { type: 'text', value: 'x' },
      { type: 'ipa', value: '/b/' },
      { type: 'ipa', value: '/c/' },
      { type: 'text', value: 'y' },
      { type: 'ipa', value: '/d/' },
    ])
  })

  it('splits a parenthetical annotation out of surrounding text', () => {
    expect(splitIntoAnnotatedSegments('record (noun)')).toEqual([
      { type: 'text', value: 'record ' },
      { type: 'paren', value: '(noun)' },
    ])
  })

  it('handles text + paren + text + ipa together, in real card-shaped input', () => {
    expect(splitIntoAnnotatedSegments('record (noun), /ˈrek.ɚd/')).toEqual([
      { type: 'text', value: 'record ' },
      { type: 'paren', value: '(noun)' },
      { type: 'text', value: ', ' },
      { type: 'ipa', value: '/ˈrek.ɚd/' },
    ])
  })

  it('handles an arbitrary mix of both kinds - paren, ipa, text, paren', () => {
    expect(splitIntoAnnotatedSegments('(a)/b/mid(c)')).toEqual([
      { type: 'paren', value: '(a)' },
      { type: 'ipa', value: '/b/' },
      { type: 'text', value: 'mid' },
      { type: 'paren', value: '(c)' },
    ])
  })
})

describe('stripAnnotations', () => {
  it('returns plain text unchanged (aside from trimming)', () => {
    expect(stripAnnotations('history')).toBe('history')
  })

  it('drops a trailing paren annotation, trimming the leftover space', () => {
    expect(stripAnnotations('record (noun)')).toBe('record')
  })

  it('drops a paren in the middle, collapsing the double space left behind', () => {
    expect(stripAnnotations('abc (note) def')).toBe('abc def')
  })

  it('returns empty for text that is pure IPA', () => {
    expect(stripAnnotations('/ˈhɪs.t̬ɚ.i/')).toBe('')
  })

  it('returns empty for text that is pure paren annotation', () => {
    expect(stripAnnotations('(noun)')).toBe('')
  })

  it('drops multiple annotations of both kinds within the same field, keeping only the real words', () => {
    expect(stripAnnotations('abc (x) /y/ def')).toBe('abc def')
  })
})
