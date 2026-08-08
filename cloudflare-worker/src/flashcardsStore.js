// KV storage layer for Flashcards - see docs/flashcards-spec.md's "Storage"
// section. One KV key per set (keyed by set name), plus one small index key
// holding just the ordered list of set names, so list_sets() never needs to
// read every set's full data. Both live in the FLASHCARDS namespace, kept
// separate from HISTORY so the two features' per-key write-rate limits never
// compete with each other.

const INDEX_KEY = 'flashcards:index'

function setKey(name) {
  return `set:${name}`
}

// Ordered list[str] of set names, oldest-first (append-only on create) - see
// docs/flashcards-spec.md's "Open questions - round 7" set-picker-order
// resolution.
export async function getIndex(env) {
  return (await env.FLASHCARDS.get(INDEX_KEY, 'json')) ?? []
}

export async function appendToIndex(env, name) {
  const index = await getIndex(env)
  await env.FLASHCARDS.put(INDEX_KEY, JSON.stringify([...index, name]))
}

export async function removeFromIndex(env, name) {
  const index = await getIndex(env)
  await env.FLASHCARDS.put(INDEX_KEY, JSON.stringify(index.filter((existing) => existing !== name)))
}

// Returns the FlashcardsSet, or null if `name` doesn't exist.
export async function getSet(env, name) {
  return await env.FLASHCARDS.get(setKey(name), 'json')
}

export async function putSet(env, name, set) {
  await env.FLASHCARDS.put(setKey(name), JSON.stringify(set))
}

export async function deleteSet(env, name) {
  await env.FLASHCARDS.delete(setKey(name))
}
