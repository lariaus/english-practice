// Flashcards' HTTP routes - see docs/flashcards-spec.md for the full API
// spec this implements (data model, error-handling rules, the
// send_review_results due-check/idempotency rule). Kept in its own module
// rather than inlined into index.js's fetch() - 10 endpoints (several
// parameterized) would make that single function unwieldy; each handler
// below still follows the same flat, router-library-free style as the rest
// of the Worker, just dispatched from its own small table instead of one
// giant if-chain.

import { appendToIndex, deleteSet, getIndex, getSet, putSet, removeFromIndex } from './flashcardsStore.js'
import { gradeReview, graduateCard } from './ankiScheduler.js'
import { parseCsv, serializeCsv } from './csvFlashcards.js'

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
  'Access-Control-Allow-Headers': 'Authorization, Content-Type',
}

function jsonResponse(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
  })
}

function csvResponse(text) {
  return new Response(text, {
    status: 200,
    headers: { ...CORS_HEADERS, 'Content-Type': 'text/csv; charset=utf-8' },
  })
}

const notFound = (message) => jsonResponse({ error: message }, 404)
const badRequest = (message) => jsonResponse({ error: message }, 400)

// Matches `/flashcards/sets/:name/cards/:uid`-style patterns against a real
// pathname - no router library, consistent with the rest of the Worker,
// just factored out since several of these routes need path params.
function matchRoute(pattern, pathname) {
  const patternParts = pattern.split('/').filter(Boolean)
  const pathParts = pathname.split('/').filter(Boolean)
  if (patternParts.length !== pathParts.length) return null

  const params = {}
  for (let i = 0; i < patternParts.length; i++) {
    const patternPart = patternParts[i]
    if (patternPart.startsWith(':')) {
      params[patternPart.slice(1)] = decodeURIComponent(pathParts[i])
    } else if (patternPart !== pathParts[i]) {
      return null
    }
  }
  return params
}

function newCard(uid, front, back) {
  return {
    uid,
    front,
    back,
    state: 'NEW',
    ease_factor: 0,
    interval_days: 0,
    due: null,
    step_index: 0,
    lapses: 0,
  }
}

async function handleListSets(env) {
  return jsonResponse(await getIndex(env))
}

async function handleCreateSet(request, env) {
  const body = await request.json().catch(() => null)
  const name = body?.name
  if (typeof name !== 'string' || !name) {
    return badRequest('name is required')
  }
  if (await getSet(env, name)) {
    return jsonResponse({ error: `set "${name}" already exists` }, 409)
  }

  const set = { name, cards: [], next_uid: 1 }
  await putSet(env, name, set)
  await appendToIndex(env, name)
  return jsonResponse(set)
}

async function handleGetSet(env, name) {
  const set = await getSet(env, name)
  if (!set) return notFound(`set "${name}" not found`)
  return jsonResponse(set)
}

async function handleDeleteSet(env, name) {
  const set = await getSet(env, name)
  if (!set) return notFound(`set "${name}" not found`)
  await deleteSet(env, name)
  await removeFromIndex(env, name)
  return jsonResponse({ status: 'ok' })
}

async function handleAddCard(request, env, setName) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  const body = await request.json().catch(() => null)
  if (!body?.front || !body?.back) {
    return badRequest('front and back are required')
  }

  const uid = set.next_uid
  set.next_uid += 1
  set.cards.push(newCard(uid, body.front, body.back))
  await putSet(env, setName, set)
  return jsonResponse(uid)
}

async function handleEditCard(request, env, setName, uidStr) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  const uid = Number(uidStr)
  const card = set.cards.find((c) => c.uid === uid)
  if (!card) return notFound(`card ${uid} not found in set "${setName}"`)

  const body = await request.json().catch(() => null)
  if (!body?.front || !body?.back) {
    return badRequest('front and back are required')
  }

  card.front = body.front
  card.back = body.back
  await putSet(env, setName, set)
  return jsonResponse({ status: 'ok' })
}

async function handleDeleteCard(env, setName, uidStr) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  const uid = Number(uidStr)
  const index = set.cards.findIndex((c) => c.uid === uid)
  if (index === -1) return notFound(`card ${uid} not found in set "${setName}"`)

  set.cards.splice(index, 1)
  await putSet(env, setName, set)
  return jsonResponse({ status: 'ok' })
}

async function handleMarkCardsLearned(request, env, setName) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  const body = await request.json().catch(() => null)
  const { today, learned } = body ?? {}
  if (typeof today !== 'string' || !Array.isArray(learned)) {
    return badRequest('today and learned are required')
  }

  // Validate every uid up front - an unknown uid errors the whole batch
  // rather than partially applying it (see docs/flashcards-spec.md's
  // "Error handling" - mark_cards_learned has no send_review_results-style
  // silent-skip carve-out).
  const cardsByUid = new Map(set.cards.map((c) => [c.uid, c]))
  for (const { uid } of learned) {
    if (!cardsByUid.has(uid)) return notFound(`card ${uid} not found in set "${setName}"`)
  }

  for (const { uid, grade } of learned) {
    Object.assign(cardsByUid.get(uid), graduateCard(grade, today))
  }

  await putSet(env, setName, set)
  return jsonResponse({ status: 'ok' })
}

async function handleGetToReviewCards(env, setName, today) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)
  if (typeof today !== 'string' || !today) return badRequest('today query param is required')

  const dueUids = set.cards.filter((c) => c.state === 'REVIEW' && c.due <= today).map((c) => c.uid)
  return jsonResponse(dueUids)
}

async function handleSendReviewResults(request, env, setName) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  const body = await request.json().catch(() => null)
  const { today, results } = body ?? {}
  if (typeof today !== 'string' || !Array.isArray(results)) {
    return badRequest('today and results are required')
  }

  const cardsByUid = new Map(set.cards.map((c) => [c.uid, c]))
  for (const { card_uid, grade, lapsed } of results) {
    const card = cardsByUid.get(card_uid)
    // Silently skip anything not currently due - a genuinely unknown uid,
    // or a known card that's already been processed (this is what makes a
    // retried submission safe - see docs/flashcards-spec.md's "Error
    // handling" section and "Open questions - round 12").
    if (!card || card.state !== 'REVIEW' || card.due > today) continue
    Object.assign(card, gradeReview(card, { grade, lapsed }, today))
  }

  await putSet(env, setName, set)
  return jsonResponse({ status: 'ok' })
}

// Puts every card's scheduling fields back to newCard()'s defaults, leaving
// front/back/uid (and the set's next_uid) untouched - "start this set's
// learning over" without losing the cards themselves.
async function handleResetSet(env, setName) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  for (const card of set.cards) {
    card.state = 'NEW'
    card.ease_factor = 0
    card.interval_days = 0
    card.due = null
    card.step_index = 0
    card.lapses = 0
  }

  await putSet(env, setName, set)
  return jsonResponse(set)
}

async function handleExportCsv(env, setName) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)
  return csvResponse(serializeCsv(set.cards))
}

async function handleImportCsv(request, env, setName) {
  const set = await getSet(env, setName)
  if (!set) return notFound(`set "${setName}" not found`)

  const csvText = await request.text()
  let rows
  try {
    rows = parseCsv(csvText)
  } catch (e) {
    return badRequest(e.message)
  }

  for (const { front, back } of rows) {
    const uid = set.next_uid
    set.next_uid += 1
    set.cards.push(newCard(uid, front, back))
  }

  await putSet(env, setName, set)
  return jsonResponse(set)
}

// Returns a Response if `request` matched a Flashcards route, or null if it
// didn't - so index.js can fall through to its own 404 for anything else.
export async function handleFlashcardsRequest(request, env, url) {
  const { pathname, searchParams } = url
  const { method } = request

  if (pathname === '/flashcards/sets' && method === 'GET') {
    return handleListSets(env)
  }
  if (pathname === '/flashcards/sets' && method === 'POST') {
    return handleCreateSet(request, env)
  }

  let params

  if ((params = matchRoute('/flashcards/sets/:name', pathname))) {
    if (method === 'GET') return handleGetSet(env, params.name)
    if (method === 'DELETE') return handleDeleteSet(env, params.name)
  }

  if ((params = matchRoute('/flashcards/sets/:name/cards', pathname)) && method === 'POST') {
    return handleAddCard(request, env, params.name)
  }

  if ((params = matchRoute('/flashcards/sets/:name/cards/:uid', pathname))) {
    if (method === 'PUT') return handleEditCard(request, env, params.name, params.uid)
    if (method === 'DELETE') return handleDeleteCard(env, params.name, params.uid)
  }

  if ((params = matchRoute('/flashcards/sets/:name/learned', pathname)) && method === 'POST') {
    return handleMarkCardsLearned(request, env, params.name)
  }

  if ((params = matchRoute('/flashcards/sets/:name/review', pathname))) {
    if (method === 'GET') return handleGetToReviewCards(env, params.name, searchParams.get('today'))
    if (method === 'POST') return handleSendReviewResults(request, env, params.name)
  }

  if ((params = matchRoute('/flashcards/sets/:name/reset', pathname)) && method === 'POST') {
    return handleResetSet(env, params.name)
  }

  if ((params = matchRoute('/flashcards/sets/:name/csv', pathname))) {
    if (method === 'GET') return handleExportCsv(env, params.name)
    if (method === 'POST') return handleImportCsv(request, env, params.name)
  }

  return null
}
