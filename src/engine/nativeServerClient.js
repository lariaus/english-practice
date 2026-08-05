// Client for native-server's /health and /subtitles endpoints (see
// native-server/ at the repo root). Purely best-effort: every function
// here resolves to null/false on any failure (not running, timeout, bad
// response) instead of throwing, so callers just skip the feature when the
// server isn't reachable - it's never required for the app to work.
//
// Deliberately relative paths, not an absolute URL with a hardcoded port:
// native-server serves this app's own static files too, so /health and
// /subtitles are always same-origin as the page itself (CLI on :8000, the
// Xcode app's embedded server on :8765, a tunnel, whatever) - no CORS, no
// port to get wrong.

import { log } from './appLog.js'

// The health check is a same-machine round trip - a bad sign if it's slow.
const HEALTH_TIMEOUT_MS = 1000
// /subtitles makes the server hit YouTube itself (list + fetch a
// transcript), which routinely takes longer than a "is it there" ping.
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

// Checked once per app lifetime, then cached - native-server is always
// local, so reachability isn't expected to change mid-session (works the
// whole session, or not at all). Shared by every caller (captions,
// StorageMap) rather than each doing its own repeated health check.
let cachedAvailability = null

export async function isNativeServerAvailable() {
  if (cachedAvailability === null) {
    cachedAvailability = (async () => {
      try {
        const response = await fetchWithTimeout('/health', HEALTH_TIMEOUT_MS)
        log('[NativeServer] health check:', response.status, response.ok ? 'ok' : 'not ok')
        return response.ok
      } catch (err) {
        log('[NativeServer] health check failed:', err.message)
        return false
      }
    })()
  }
  return cachedAvailability
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
