// A read-only offline fallback for Flashcards, backed by StorageMap (see
// docs/local-storage.md) - the DB is always tried first; this is only ever
// consulted on a genuine connectivity failure (flashcardsClient.js's
// `error.offline`), never on a real server error. Deliberately caches only
// front/back content, never scheduling fields (state/ease_factor/
// interval_days/due/step_index/lapses) - none of that matters for a
// read-only offline view, and it means mark_cards_learned/
// send_review_results (which only ever touch scheduling fields) never
// need to write anything here at all.
//
// Every write here is a full replace of whatever unit is being written
// (a whole set's cards, or one card's front/back) - never a merge against
// what was previously cached. A merge could leave stale cards lingering
// that were legitimately deleted server-side (e.g. from another device);
// a full replace can't have that problem.
import { StorageMap } from './storageMap.js'
import {
  addCard,
  createSet,
  deleteCard,
  deleteSet,
  editCard,
  getSet,
  importCsv,
  listSets,
} from './flashcardsClient.js'

const map = StorageMap.get('flashcards')
const SET_LIST_KEY = 'set-list'
const setKey = (name) => `set:${name}`

// list[{uid, front, back}] - the only fields a read-only offline view
// needs.
function reduceCards(cards) {
  return cards.map((c) => ({ uid: c.uid, front: c.front, back: c.back }))
}

async function getCachedSetList() {
  return (await map.get(SET_LIST_KEY)) ?? []
}

async function getCachedSet(name) {
  return await map.get(setKey(name))
}

// {names, offline} - offline is true only when the result actually came
// from the cache (a real server error still throws, same as listSets()
// alone would).
export async function listSetsOrCached() {
  try {
    const names = await listSets()
    await map.set(SET_LIST_KEY, names)
    return { names, offline: false }
  } catch (e) {
    if (!e.offline) throw e
    const cached = await getCachedSetList()
    if (cached.length === 0) throw e
    return { names: cached, offline: true }
  }
}

// {set, offline} - `set.cards` is the reduced {uid,front,back} shape only
// when offline is true; the real FlashcardsSet shape otherwise.
export async function getSetOrCached(name) {
  try {
    const set = await getSet(name)
    await map.set(setKey(name), { name, cards: reduceCards(set.cards) })
    return { set, offline: false }
  } catch (e) {
    if (!e.offline) throw e
    const cached = await getCachedSet(name)
    if (!cached) throw e
    return { set: cached, offline: true }
  }
}

export async function createSetCached(name) {
  const set = await createSet(name)
  try {
    const names = await getCachedSetList()
    await map.set(SET_LIST_KEY, [...names, name])
    await map.set(setKey(name), { name, cards: [] })
  } catch {
    // Best-effort - the real create already succeeded, a cache-write
    // failure here must never surface as a user-facing error.
  }
  return set
}

export async function deleteSetCached(name) {
  await deleteSet(name)
  try {
    const names = await getCachedSetList()
    await map.set(
      SET_LIST_KEY,
      names.filter((n) => n !== name),
    )
    await map.delete(setKey(name))
  } catch {
    // Best-effort, see createSetCached().
  }
}

export async function addCardCached(setName, { front, back }) {
  const uid = await addCard(setName, { front, back })
  try {
    const cached = (await getCachedSet(setName)) ?? { name: setName, cards: [] }
    cached.cards.push({ uid, front, back })
    await map.set(setKey(setName), cached)
  } catch {
    // Best-effort, see createSetCached().
  }
  return uid
}

export async function editCardCached(setName, uid, { front, back }) {
  const result = await editCard(setName, uid, { front, back })
  try {
    const cached = await getCachedSet(setName)
    const card = cached?.cards.find((c) => c.uid === uid)
    if (card) {
      card.front = front
      card.back = back
      await map.set(setKey(setName), cached)
    }
  } catch {
    // Best-effort, see createSetCached().
  }
  return result
}

export async function deleteCardCached(setName, uid) {
  const result = await deleteCard(setName, uid)
  try {
    const cached = await getCachedSet(setName)
    if (cached) {
      cached.cards = cached.cards.filter((c) => c.uid !== uid)
      await map.set(setKey(setName), cached)
    }
  } catch {
    // Best-effort, see createSetCached().
  }
  return result
}

export async function importCsvCached(setName, csvText) {
  const set = await importCsv(setName, csvText)
  try {
    await map.set(setKey(setName), { name: setName, cards: reduceCards(set.cards) })
  } catch {
    // Best-effort, see createSetCached().
  }
  return set
}
