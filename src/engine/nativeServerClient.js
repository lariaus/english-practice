// Client for native-server's /subtitles endpoint (see native-server/ at
// the repo root). Purely best-effort: resolves to null on any failure
// (timeout, bad response) instead of throwing, so callers just skip
// captions rather than fail outright.
//
// Deliberately a relative path, not an absolute URL with a hardcoded port:
// native-server serves this app's own static files too, so /subtitles is
// always same-origin as the page itself (CLI on :8000, the Xcode app's
// embedded server on :8765, a tunnel, whatever) - no CORS, no port to get
// wrong.

import { log } from './appLog.js'

// /subtitles makes the server hit YouTube itself (list + fetch a
// transcript), which routinely takes longer than a simple ping.
const SUBTITLES_TIMEOUT_MS = 10000

async function fetchWithTimeout(url, timeoutMs) {
  const controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), timeoutMs)
  try {
    return await fetch(url, { signal: controller.signal })
  } finally {
    clearTimeout(timeout)
  }
}

// Returns { videoId, language, languageCode, isGenerated, cues } or null.
export async function fetchSubtitles(youtubeUrl, lang = 'en') {
  const params = new URLSearchParams({ url: youtubeUrl, lang })
  const requestUrl = `/subtitles?${params}`
  try {
    const response = await fetchWithTimeout(requestUrl, SUBTITLES_TIMEOUT_MS)
    if (!response.ok) {
      const body = await response.json().catch(() => null)
      log('[NativeServer] subtitles request failed:', response.status, body?.error)
      return null
    }
    const data = await response.json()
    if (!Array.isArray(data.cues)) {
      log('[NativeServer] subtitles response missing cues array:', data)
      return null
    }
    log(
      '[NativeServer] subtitles fetched:',
      data.languageCode,
      data.isGenerated ? '(auto-generated)' : '(manual)',
      '-',
      data.cues.length,
      'cues',
    )
    return data
  } catch (err) {
    log('[NativeServer] subtitles request errored:', requestUrl, err.message)
    return null
  }
}

// Bulk vocabulary filter for ShadowLoopMode (see
// docs/shadow-loop-mode-spec.md) - given a word list, returns just the
// subset that already has a cached, real US audio recording. Deliberately
// cache-only server-side (never touches the network for any word), so this
// call itself should always resolve quickly - best-effort like the rest of
// this file: resolves to [] on any failure rather than throwing, since an
// empty result here just means "nothing to loop" for the caller, not a
// broken screen.
export async function fetchUsAudioWords(words) {
  try {
    const response = await fetch('/dictionary/us-audio-words', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ words }),
    })
    if (!response.ok) {
      log('[NativeServer] us-audio-words request failed:', response.status)
      return []
    }
    const data = await response.json()
    return Array.isArray(data.words) ? data.words : []
  } catch (err) {
    log('[NativeServer] us-audio-words request errored:', err.message)
    return []
  }
}
