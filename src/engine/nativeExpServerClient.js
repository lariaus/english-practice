// Client for the optional NativeExpServer companion process (see
// native-exp-server/ at the repo root). Purely best-effort: every function
// here resolves to null/false on any failure (not running, timeout, bad
// response) instead of throwing, so callers just skip the feature when the
// server isn't reachable - it's never required for the app to work.

const NATIVE_EXP_SERVER_PORT = 5905
// The health check is a same-machine round trip - a bad sign if it's slow.
const HEALTH_TIMEOUT_MS = 1000
// /subtitles makes the Python server hit YouTube itself (list + fetch a
// transcript), which routinely takes longer than a "is it there" ping.
const SUBTITLES_TIMEOUT_MS = 10000

function baseUrl() {
  return `http://${window.location.hostname}:${NATIVE_EXP_SERVER_PORT}`
}

async function fetchWithTimeout(url, timeoutMs) {
  const controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), timeoutMs)
  try {
    return await fetch(url, { signal: controller.signal })
  } finally {
    clearTimeout(timeout)
  }
}

export async function isNativeExpServerAvailable() {
  try {
    const response = await fetchWithTimeout(`${baseUrl()}/health`, HEALTH_TIMEOUT_MS)
    console.log('[NativeExpServer] health check:', response.status, response.ok ? 'ok' : 'not ok')
    return response.ok
  } catch (err) {
    console.log('[NativeExpServer] health check failed:', err.message)
    return false
  }
}

// Returns { videoId, language, languageCode, isGenerated, cues } or null.
export async function fetchSubtitles(youtubeUrl, lang = 'en') {
  const params = new URLSearchParams({ url: youtubeUrl, lang })
  const requestUrl = `${baseUrl()}/subtitles?${params}`
  try {
    const response = await fetchWithTimeout(requestUrl, SUBTITLES_TIMEOUT_MS)
    if (!response.ok) {
      const body = await response.json().catch(() => null)
      console.log('[NativeExpServer] subtitles request failed:', response.status, body?.error)
      return null
    }
    const data = await response.json()
    if (!Array.isArray(data.cues)) {
      console.log('[NativeExpServer] subtitles response missing cues array:', data)
      return null
    }
    console.log(
      '[NativeExpServer] subtitles fetched:',
      data.languageCode,
      data.isGenerated ? '(auto-generated)' : '(manual)',
      '-',
      data.cues.length,
      'cues',
    )
    return data
  } catch (err) {
    console.log('[NativeExpServer] subtitles request errored:', requestUrl, err.message)
    return null
  }
}
