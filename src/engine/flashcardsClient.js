// Flashcards' Cloudflare Worker client - see docs/flashcards-spec.md for
// the full API this wraps, and cloudflare-worker/README.md's "Flashcards"
// section for the exact routes/bodies.
//
// Deliberately different from ytHistory.js's convention: that file treats
// every failure as "silently fall back to empty," because history sync is
// a nice-to-have. Flashcards has no offline mode at all - the spec is
// explicit that it "requires a configured sync server to do anything at
// all" - so every function here throws on failure instead, with a message
// good enough to show the user directly, rather than returning something
// a screen could mistake for "you have no sets"/"nothing due."

import { getSyncServerUrl } from './syncConfig.js'

// A genuine connectivity failure (no server configured, or fetch() itself
// failed) - distinct from a real error response from a reachable server,
// which is a normal Error. Callers that fall back to cached data on
// connectivity loss (see flashcardsOfflineCache.js) check this flag rather
// than treating every failure the same - a real 404/500 should surface as
// an error, never as a reason to quietly serve stale data.
function offlineError(message) {
  const error = new Error(message)
  error.offline = true
  return error
}

async function request(path, options) {
  const serverUrl = await getSyncServerUrl()
  if (!serverUrl) {
    throw offlineError('No sync server configured - add one in Settings first.')
  }

  let response
  try {
    response = await fetch(`${serverUrl}${path}`, options)
  } catch {
    throw offlineError('Could not reach the sync server. Check your connection and the URL in Settings.')
  }

  const data = await response.json().catch(() => null)
  if (!response.ok) {
    throw new Error(data?.error || `Sync server error (${response.status})`)
  }
  return data
}

// Same shape as request(), but for an endpoint whose *response* is raw text
// (CSV export) rather than JSON - see docs/flashcards-spec.md's "CSV
// import/export" section.
async function requestText(path, options) {
  const serverUrl = await getSyncServerUrl()
  if (!serverUrl) {
    throw offlineError('No sync server configured - add one in Settings first.')
  }

  let response
  try {
    response = await fetch(`${serverUrl}${path}`, options)
  } catch {
    throw offlineError('Could not reach the sync server. Check your connection and the URL in Settings.')
  }

  if (!response.ok) {
    const data = await response.json().catch(() => null)
    throw new Error(data?.error || `Sync server error (${response.status})`)
  }
  return response.text()
}

function jsonBody(body) {
  return { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }
}

export function listSets() {
  return request('/flashcards/sets')
}

export function getSet(name) {
  return request(`/flashcards/sets/${encodeURIComponent(name)}`)
}

export function createSet(name) {
  return request('/flashcards/sets', jsonBody({ name }))
}

export function deleteSet(name) {
  return request(`/flashcards/sets/${encodeURIComponent(name)}`, { method: 'DELETE' })
}

// Puts every card in the set back to NEW, wiping all learning/review
// progress - front/back and the cards themselves are untouched. Returns
// the updated FlashcardsSet.
export function resetSet(name) {
  return request(`/flashcards/sets/${encodeURIComponent(name)}/reset`, { method: 'POST' })
}

export function addCard(setName, { front, back }) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/cards`, jsonBody({ front, back }))
}

export function editCard(setName, uid, { front, back }) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/cards/${uid}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ front, back }),
  })
}

export function deleteCard(setName, uid) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/cards/${uid}`, { method: 'DELETE' })
}

// `learned` is list[{uid, grade: 'GOOD'|'EASY'}] - see docs/flashcards-spec.md's Learning section.
export function markCardsLearned(setName, today, learned) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/learned`, jsonBody({ today, learned }))
}

export function getToReviewCards(setName, today) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/review?today=${encodeURIComponent(today)}`)
}

// `results` is list[{card_uid, grade: 'HARD'|'GOOD'|'EASY', lapsed}] - see docs/flashcards-spec.md's Review section.
export function sendReviewResults(setName, today, results) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/review`, jsonBody({ today, results }))
}

// Returns the raw CSV text directly - no client-side CSV building at all,
// see docs/flashcards-spec.md's "CSV import/export" section.
export function exportCsv(setName) {
  return requestText(`/flashcards/sets/${encodeURIComponent(setName)}/csv`)
}

// `csvText` is sent as-is (the raw file content) - no client-side parsing
// either, the server does all of it. Returns the updated FlashcardsSet.
export function importCsv(setName, csvText) {
  return request(`/flashcards/sets/${encodeURIComponent(setName)}/csv`, {
    method: 'POST',
    headers: { 'Content-Type': 'text/csv' },
    body: csvText,
  })
}
