// Fetches word info (phonetics, definitions) for the click-a-word popup,
// shared across the app (not just YT Shadowing). Backed by native-server's
// own /dictionary route (see native-server/native_server_core/lib/
// dictionary_route.cpp) rather than calling a public dictionary API
// directly - that used to be api.dictionaryapi.dev, which turned out to be
// permanently dead (unmaintained since 2023) and CORS-blocked whenever it
// did respond. native-server now does the real lookup itself (Free
// Dictionary API + Wiktionary, merged) and caches pronunciation audio to
// disk, serving it back at /server_data/<path>. The server returns every
// phonetics row unfiltered, in source order - finalizePhoneticsRows below
// (deduping, capping unlabeled rows, US-first sorting) is a display concern
// handled here rather than server-side. Caches results in memory so
// re-looking-up the same word never re-fetches for the rest of the session
// (only completed, non-fast lookups are cached, not in-flight promises - a
// known, deliberately-deferred inefficiency: two near-simultaneous callers
// for the same word each trigger their own request). A `fast` result is
// never cached - see fetchWordInfo below. Each phonetic entry carries
// its own audio URL (when available) - actually playing it is the view
// layer's job.

import { log } from './appLog.js'

const cache = new Map()

// Generous, but not literally worst-case-safe: the server can issue up to
// ~5 sequential upstream HTTP calls on a cache miss (Free Dictionary API,
// up to 3 Wiktionary calls, plus an audio download), each with its own
// independent 15s server-side timeout - a pathologically slow-but-not-
// quite-failing chain could still exceed this. Accepted for now, same
// trade-off nativeServerClient.js's own SUBTITLES_TIMEOUT_MS already makes
// for a similar multi-step server call - revisit if the server side ever
// parallelizes those calls.
const DICTIONARY_TIMEOUT_MS = 25000

// Small, fully generic AbortController wrapper - duplicated here rather
// than imported from nativeServerClient.js, matching this codebase's
// convention of duplicating small, self-contained, stateless helpers
// rather than centralizing them.
async function fetchWithTimeout(url, timeoutMs) {
  const controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), timeoutMs)
  try {
    return await fetch(url, { signal: controller.signal })
  } finally {
    clearTimeout(timeout)
  }
}

// Display-side cleanup of the raw phonetics list the server returns
// (native-server deliberately returns every row unfiltered, in source
// order - see dictionary_entry_detail.h): dedup on exact (text, label)
// pairs, then cap unlabeled ("unknown accent") rows to just the first one
// seen - some words (e.g. "people") list many near-identical unlabeled
// transcriptions (dialectal spelling variants, alternate romanizations)
// that are noise once a labeled US/UK/AU row already exists - and always
// place that one surviving unlabeled row after every labeled row,
// US-labeled row(s) sorted first.
function finalizePhoneticsRows(rows) {
  const deduped = []
  for (const row of rows) {
    const isDuplicate = deduped.some((d) => d.text === row.text && d.label === row.label)
    if (!isDuplicate) deduped.push(row)
  }

  const labeled = []
  let firstUnlabeled = null
  for (const row of deduped) {
    if (!row.label) {
      if (!firstUnlabeled) firstUnlabeled = row
    } else {
      labeled.push(row)
    }
  }

  // Array.prototype.sort is stable (ECMAScript 2019+, every evergreen
  // browser) - non-US rows keep their relative source order.
  labeled.sort((a, b) => (b.label === 'US') - (a.label === 'US'))

  if (firstUnlabeled) labeled.push(firstUnlabeled)

  return labeled
}

// Returns a normalized { word, phonetics, usPhonetics, meanings, sourceUrl,
// license } object, or null if nothing was found (unknown word, native-
// server unreachable, network error, timeout, malformed response, etc).
//
// `fast`, when true, asks the server for its cheap fastFetch mode (Free
// Dictionary API only - no Wiktionary lookup, no real pronunciation audio,
// no server-side entry-cache write) - see DictionaryPopup.vue's two-phase
// loading. A fast result is deliberately never written to `cache` below, so
// a later non-fast call for the same word still does the real lookup - a
// pre-existing full cache entry, though, still short-circuits a fast
// request too, since that's strictly better data anyway.
export async function fetchWordInfo(word, lang = 'en', { fast = false } = {}) {
  const key = `${lang}:${word.trim().toLowerCase()}`
  if (cache.has(key)) return cache.get(key)

  const params = new URLSearchParams({ word, lang, fast: fast ? 'true' : 'false' })
  const requestUrl = `/dictionary?${params}`

  let data
  try {
    const response = await fetchWithTimeout(requestUrl, DICTIONARY_TIMEOUT_MS)
    if (!response.ok) {
      const body = await response.json().catch(() => null)
      log('[Dictionary] request failed:', response.status, body?.error)
      return null
    }
    data = await response.json()
  } catch (err) {
    log('[Dictionary] request errored:', requestUrl, err.message)
    return null
  }

  const phonetics = finalizePhoneticsRows(
    (data.phonetics || []).map((p) => ({
      text: p.text,
      label: p.label,
      audio: p.audio ? `/server_data/${p.audio}` : null,
    }))
  )

  const result = {
    word: data.word,
    phonetics,
    usPhonetics: phonetics.find((p) => p.label === 'US') || null,
    meanings: (data.meanings || []).map((meaning) => ({
      partOfSpeech: meaning.partOfSpeech,
      definitions: (meaning.definitions || []).map((def) => ({
        definition: def.definition,
        example: def.examples?.[0] || null,
        synonyms: def.synonyms || [],
      })),
    })),
    sourceUrl: data.sourceUrl || null,
    license: data.license || null,
  }

  if (!fast) cache.set(key, result)
  return result
}
