// Direct tests of the storage module itself - calling putSet/getSet/
// appendToIndex against the real KV binding directly, with no HTTP request/
// response or JSON body parsing involved at all. Complements
// flashcardsRoutes.test.js's full API-level round-trips: if a UTF-8 bug
// ever showed up in only one of the two test files, that would pinpoint
// whether it's the storage layer or the HTTP/routing layer at fault.
import { env } from 'cloudflare:test'
import { describe, expect, it } from 'vitest'
import { appendToIndex, getIndex, getSet, putSet } from '../src/flashcardsStore.js'

let setCounter = 0
function uniqueSetName() {
  setCounter += 1
  return `direct-test-set-${setCounter}`
}

describe('flashcardsStore - direct KV round-trips', () => {
  it('putSet/getSet round-trips a set exactly, including Unicode card content', async () => {
    const name = uniqueSetName()
    const set = {
      name,
      next_uid: 2,
      cards: [
        {
          uid: 1,
          front: 'déjà vu, château, garçon', // French accents/cedilla
          back: '/deɪʒɑː vuː/ ˈʃæt.oʊ ɡɑʁˈsɔ̃', // IPA, incl. combining tilde
          state: 'NEW',
          ease_factor: 0,
          interval_days: 0,
          due: null,
          step_index: 0,
          lapses: 0,
        },
      ],
    }

    await putSet(env, name, set)
    const loaded = await getSet(env, name)

    expect(loaded).toEqual(set)
  })

  it('getSet returns null for a set that was never put', async () => {
    expect(await getSet(env, uniqueSetName())).toBeNull()
  })

  it('appendToIndex/getIndex round-trips a Unicode set name exactly', async () => {
    const name = `日本語-${uniqueSetName()}` // Japanese, a non-Latin script - different case than French/IPA's accented-Latin
    await appendToIndex(env, name)

    const index = await getIndex(env)
    expect(index).toContain(name)
    // Exact string equality, not just "contains something similar" -
    // catches subtle mangling (e.g. normalization form changes) that a
    // looser check would miss.
    expect(index.find((existing) => existing === name)).toBe(name)
  })
})
