// The Cloudflare Worker's URL (see cloudflare-worker/) - deliberately never
// hardcoded here or anywhere else in source. Entered manually per device via
// the Settings screen instead, and persisted in localStorage from there on.
// Every sync feature (ytHistory.js today, more later) reads it through here
// rather than touching localStorage directly.

const SYNC_SERVER_URL_KEY = 'sync-server-url'

export function getSyncServerUrl() {
  try {
    return localStorage.getItem(SYNC_SERVER_URL_KEY) || ''
  } catch {
    return ''
  }
}

export function setSyncServerUrl(url) {
  try {
    const trimmed = url.trim().replace(/\/+$/, '')
    if (trimmed) {
      localStorage.setItem(SYNC_SERVER_URL_KEY, trimmed)
    } else {
      localStorage.removeItem(SYNC_SERVER_URL_KEY)
    }
  } catch {
    // ignore - e.g. storage disabled
  }
}
