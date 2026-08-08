// Miniflare integration tests - these actually touch KV/HTTP, unlike
// ankiScheduler.test.js's plain unit tests. Each test uses its own
// unique set name, per docs/flashcards-spec.md's "Testing" section.
import { SELF } from 'cloudflare:test'
import { beforeEach, describe, expect, it } from 'vitest'

let setCounter = 0
function uniqueSetName() {
  setCounter += 1
  return `test-set-${setCounter}`
}

async function createSet(name) {
  return SELF.fetch('https://example.com/flashcards/sets', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name }),
  })
}

async function addCard(name, { front = 'front', back = 'back' } = {}) {
  const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ front, back }),
  })
  return response.json()
}

describe('list_sets / create_set / get_set / delete_set', () => {
  it('list_sets returns created set names, in creation order', async () => {
    const a = uniqueSetName()
    const b = uniqueSetName()
    await createSet(a)
    await createSet(b)

    const response = await SELF.fetch('https://example.com/flashcards/sets')
    const names = await response.json()
    expect(names.indexOf(a)).toBeLessThan(names.indexOf(b))
  })

  it('create_set returns the new set with next_uid starting at 1 and no cards', async () => {
    const name = uniqueSetName()
    const response = await createSet(name)
    expect(response.status).toBe(200)
    expect(await response.json()).toEqual({ name, cards: [], next_uid: 1 })
  })

  it('create_set errors on a name collision', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const response = await createSet(name)
    expect(response.status).toBe(409)
  })

  it('get_set 404s for an unknown set', async () => {
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${uniqueSetName()}`)
    expect(response.status).toBe(404)
  })

  it('delete_set removes it from both storage and the index', async () => {
    const name = uniqueSetName()
    await createSet(name)

    const deleteResponse = await SELF.fetch(`https://example.com/flashcards/sets/${name}`, { method: 'DELETE' })
    expect(deleteResponse.status).toBe(200)

    expect((await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).status).toBe(404)
    const names = await (await SELF.fetch('https://example.com/flashcards/sets')).json()
    expect(names).not.toContain(name)
  })

  it('delete_set 404s for an unknown set', async () => {
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${uniqueSetName()}`, { method: 'DELETE' })
    expect(response.status).toBe(404)
  })
})

describe('add_card / edit_card / delete_card', () => {
  it('add_card returns a server-assigned uid and appends a NEW card', async () => {
    const name = uniqueSetName()
    await createSet(name)

    const uid = await addCard(name, { front: 'hello', back: 'bonjour' })
    expect(uid).toBe(1)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    expect(set.cards).toEqual([
      { uid: 1, front: 'hello', back: 'bonjour', state: 'NEW', ease_factor: 0, interval_days: 0, due: null, step_index: 0, lapses: 0 },
    ])
  })

  it('uid never gets reused, even after deleting the highest-numbered card', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const first = await addCard(name)
    await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards/${first}`, { method: 'DELETE' })
    const second = await addCard(name)
    expect(second).toBe(first + 1)
  })

  it('add_card 400s when front/back are missing', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ front: 'only front' }),
    })
    expect(response.status).toBe(400)
  })

  it('add_card 404s for an unknown set', async () => {
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${uniqueSetName()}/cards`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ front: 'a', back: 'b' }),
    })
    expect(response.status).toBe(404)
  })

  it('edit_card updates content fields only, never scheduling fields', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await addCard(name)

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards/${uid}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ front: 'edited front', back: 'edited back' }),
    })
    expect(response.status).toBe(200)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    const card = set.cards.find((c) => c.uid === uid)
    expect(card.front).toBe('edited front')
    expect(card.back).toBe('edited back')
    expect(card.state).toBe('NEW')
  })

  it('edit_card 404s for an unknown uid', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards/999`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ front: 'a', back: 'b' }),
    })
    expect(response.status).toBe(404)
  })

  it('delete_card removes only that card', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await addCard(name)

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards/${uid}`, { method: 'DELETE' })
    expect(response.status).toBe(200)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    expect(set.cards).toEqual([])
  })

  it('delete_card 404s for an unknown uid', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards/999`, { method: 'DELETE' })
    expect(response.status).toBe(404)
  })
})

describe('mark_cards_learned', () => {
  it('graduates GOOD to a 1-day interval and EASY to a 4-day interval, both at starting ease', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const goodUid = await addCard(name)
    const easyUid = await addCard(name)

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/learned`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        today: '2026-01-01',
        learned: [
          { uid: goodUid, grade: 'GOOD' },
          { uid: easyUid, grade: 'EASY' },
        ],
      }),
    })
    expect(response.status).toBe(200)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    const good = set.cards.find((c) => c.uid === goodUid)
    const easy = set.cards.find((c) => c.uid === easyUid)
    expect(good).toMatchObject({ state: 'REVIEW', ease_factor: 2.5, interval_days: 1, due: '2026-01-02' })
    expect(easy).toMatchObject({ state: 'REVIEW', ease_factor: 2.5, interval_days: 4, due: '2026-01-05' })
  })

  it('errors the whole batch (applying nothing) if any uid is unknown', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const validUid = await addCard(name)

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/learned`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        today: '2026-01-01',
        learned: [
          { uid: validUid, grade: 'GOOD' },
          { uid: 999, grade: 'GOOD' },
        ],
      }),
    })
    expect(response.status).toBe(404)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    expect(set.cards.find((c) => c.uid === validUid).state).toBe('NEW')
  })
})

describe('reset_set', () => {
  it('puts every card back to NEW defaults, keeping front/back/uid', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await addCard(name, { front: 'hello', back: 'bonjour' })
    await SELF.fetch(`https://example.com/flashcards/sets/${name}/learned`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ today: '2026-01-01', learned: [{ uid, grade: 'EASY' }] }),
    })

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/reset`, { method: 'POST' })
    expect(response.status).toBe(200)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    expect(set.cards).toEqual([
      { uid, front: 'hello', back: 'bonjour', state: 'NEW', ease_factor: 0, interval_days: 0, due: null, step_index: 0, lapses: 0 },
    ])
  })

  it('does not touch next_uid, so future uids keep incrementing normally', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const first = await addCard(name)
    await SELF.fetch(`https://example.com/flashcards/sets/${name}/reset`, { method: 'POST' })
    const second = await addCard(name)
    expect(second).toBe(first + 1)
  })

  it('404s for an unknown set', async () => {
    const response = await SELF.fetch(`https://example.com/flashcards/sets/${uniqueSetName()}/reset`, { method: 'POST' })
    expect(response.status).toBe(404)
  })
})

describe('get_to_review_cards / send_review_results', () => {
  async function setUpDueCard(name, today) {
    const uid = await addCard(name)
    await SELF.fetch(`https://example.com/flashcards/sets/${name}/learned`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ today, learned: [{ uid, grade: 'GOOD' }] }),
    })
    return uid
  }

  it('only returns REVIEW cards that are actually due, excluding NEW and not-yet-due cards', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const dueUid = await setUpDueCard(name, '2026-01-01') // due 2026-01-02
    await addCard(name) // stays NEW, never due

    const notYetDue = await (
      await SELF.fetch(`https://example.com/flashcards/sets/${name}/review?today=2026-01-01`)
    ).json()
    expect(notYetDue).toEqual([])

    const dueToday = await (
      await SELF.fetch(`https://example.com/flashcards/sets/${name}/review?today=2026-01-02`)
    ).json()
    expect(dueToday).toEqual([dueUid])
  })

  it('applies grading and moves due forward', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await setUpDueCard(name, '2026-01-01') // due 2026-01-02, interval 1, ease 2.5

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/review`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        today: '2026-01-02',
        results: [{ card_uid: uid, grade: 'GOOD', lapsed: false }],
      }),
    })
    expect(response.status).toBe(200)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    const card = set.cards.find((c) => c.uid === uid)
    // interval was 1, ease 2.5 -> round(1 * 2.5) = 3 (round-half-up), due = 2026-01-02 + 3
    expect(card.interval_days).toBe(3)
    expect(card.due).toBe('2026-01-05')
  })

  it('a lapsed result applies the ease penalty and increments lapses exactly once', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await setUpDueCard(name, '2026-01-01')

    await SELF.fetch(`https://example.com/flashcards/sets/${name}/review`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        today: '2026-01-02',
        results: [{ card_uid: uid, grade: 'GOOD', lapsed: true }],
      }),
    })

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    const card = set.cards.find((c) => c.uid === uid)
    expect(card.ease_factor).toBe(2.3)
    expect(card.interval_days).toBe(1)
    expect(card.lapses).toBe(1)
  })

  it('resubmitting the same batch a second time is a safe no-op (retry idempotency)', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await setUpDueCard(name, '2026-01-01')

    const submit = () =>
      SELF.fetch(`https://example.com/flashcards/sets/${name}/review`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          today: '2026-01-02',
          results: [{ card_uid: uid, grade: 'GOOD', lapsed: false }],
        }),
      })

    await submit()
    const afterFirst = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()

    const secondResponse = await submit()
    expect(secondResponse.status).toBe(200)
    const afterSecond = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()

    expect(afterSecond).toEqual(afterFirst)
  })

  it('send_review_results silently ignores an unknown card_uid rather than erroring', async () => {
    const name = uniqueSetName()
    await createSet(name)

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/review`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        today: '2026-01-02',
        results: [{ card_uid: 999, grade: 'GOOD', lapsed: false }],
      }),
    })
    expect(response.status).toBe(200)
  })
})

describe('UTF-8 / Unicode content', () => {
  // French accents/cedilla plus IPA phonetic characters, including a
  // combining tilde (two codepoints forming one visual character) - a
  // meaningfully harder case than plain ASCII or single-codepoint accents.
  const FRENCH_FRONT = 'déjà vu, château, garçon'
  const IPA_BACK = '/deɪʒɑː vuː/ ˈʃæt.oʊ ɡɑʁˈsɔ̃'

  it('add_card / get_set round-trips front/back exactly (JSON body -> KV -> JSON response)', async () => {
    const name = uniqueSetName()
    await createSet(name)

    const uid = await addCard(name, { front: FRENCH_FRONT, back: IPA_BACK })

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    const card = set.cards.find((c) => c.uid === uid)
    expect(card.front).toBe(FRENCH_FRONT)
    expect(card.back).toBe(IPA_BACK)
  })

  it('edit_card round-trips updated Unicode content exactly (separate handler/validation path)', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const uid = await addCard(name)

    const response = await SELF.fetch(`https://example.com/flashcards/sets/${name}/cards/${uid}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ front: FRENCH_FRONT, back: IPA_BACK }),
    })
    expect(response.status).toBe(200)

    const set = await (await SELF.fetch(`https://example.com/flashcards/sets/${name}`)).json()
    const card = set.cards.find((c) => c.uid === uid)
    expect(card.front).toBe(FRENCH_FRONT)
    expect(card.back).toBe(IPA_BACK)
  })

  it('a Unicode set name round-trips through URL-path encoding, not just JSON bodies', async () => {
    // Different mechanism than front/back (which travel in the JSON body) -
    // the set name travels as a URL path segment, so this exercises
    // encodeURIComponent/decodeURIComponent (flashcardsClient.js/
    // flashcardsRoutes.js's matchRoute) rather than JSON (de)serialization.
    const name = `Français-${uniqueSetName()}`

    const createResponse = await createSet(name)
    expect(createResponse.status).toBe(200)
    expect((await createResponse.json()).name).toBe(name)

    const getResponse = await SELF.fetch(`https://example.com/flashcards/sets/${encodeURIComponent(name)}`)
    expect(getResponse.status).toBe(200)
    expect((await getResponse.json()).name).toBe(name)

    const names = await (await SELF.fetch('https://example.com/flashcards/sets')).json()
    expect(names).toContain(name)

    const deleteResponse = await SELF.fetch(`https://example.com/flashcards/sets/${encodeURIComponent(name)}`, {
      method: 'DELETE',
    })
    expect(deleteResponse.status).toBe(200)
  })
})

describe('CSV export / import', () => {
  async function exportCsv(name) {
    return SELF.fetch(`https://example.com/flashcards/sets/${name}/csv`)
  }

  async function importCsv(name, csvText) {
    return SELF.fetch(`https://example.com/flashcards/sets/${name}/csv`, {
      method: 'POST',
      headers: { 'Content-Type': 'text/csv' },
      body: csvText,
    })
  }

  it('exports just Front/Back, dropping all scheduling fields', async () => {
    const name = uniqueSetName()
    await createSet(name)
    await addCard(name, { front: 'Bleu', back: 'Blue' })
    await addCard(name, { front: 'Rouge', back: 'Red' })

    const response = await exportCsv(name)
    expect(response.status).toBe(200)
    expect(response.headers.get('Content-Type')).toMatch(/text\/csv/)
    expect(await response.text()).toBe('Front,Back\r\nBleu,Blue\r\nRouge,Red\r\n')
  })

  it('exports just the header for an empty set', async () => {
    const name = uniqueSetName()
    await createSet(name)
    expect(await (await exportCsv(name)).text()).toBe('Front,Back\r\n')
  })

  it('export 404s for an unknown set', async () => {
    expect((await exportCsv(uniqueSetName())).status).toBe(404)
  })

  it('imports cards from a CSV with just Front/Back columns', async () => {
    const name = uniqueSetName()
    await createSet(name)

    const response = await importCsv(name, 'Front,Back,\nBleu,Blue,\nRouge,Red,\n')
    expect(response.status).toBe(200)
    const set = await response.json()

    expect(set.cards).toEqual([
      { uid: 1, front: 'Bleu', back: 'Blue', state: 'NEW', ease_factor: 0, interval_days: 0, due: null, step_index: 0, lapses: 0 },
      { uid: 2, front: 'Rouge', back: 'Red', state: 'NEW', ease_factor: 0, interval_days: 0, due: null, step_index: 0, lapses: 0 },
    ])
    expect(set.next_uid).toBe(3)
  })

  it('imported uids continue from next_uid rather than restarting, alongside existing cards', async () => {
    const name = uniqueSetName()
    await createSet(name)
    await addCard(name) // uid 1

    const response = await importCsv(name, 'Front,Back\nBleu,Blue\n')
    const set = await response.json()
    expect(set.cards.map((c) => c.uid)).toEqual([1, 2])
  })

  it('import 400s with a clear error when the header is missing Front/Back', async () => {
    const name = uniqueSetName()
    await createSet(name)
    const response = await importCsv(name, 'Question,Answer\nBleu,Blue\n')
    expect(response.status).toBe(400)
    expect((await response.json()).error).toMatch(/Front.*Back/)
  })

  it('import 404s for an unknown set', async () => {
    const response = await importCsv(uniqueSetName(), 'Front,Back\nBleu,Blue\n')
    expect(response.status).toBe(404)
  })

  it('round-trips export then import into a fresh set, preserving front/back exactly', async () => {
    const sourceName = uniqueSetName()
    await createSet(sourceName)
    await addCard(sourceName, { front: 'déjà vu, château', back: '/deɪʒɑː vuː/' })

    const csv = await (await exportCsv(sourceName)).text()

    const destName = uniqueSetName()
    await createSet(destName)
    const importResponse = await importCsv(destName, csv)
    const destSet = await importResponse.json()

    expect(destSet.cards).toHaveLength(1)
    expect(destSet.cards[0].front).toBe('déjà vu, château')
    expect(destSet.cards[0].back).toBe('/deɪʒɑː vuː/')
  })
})
