// The Cloudflare Worker's URL (see cloudflare-worker/) - deliberately never
// hardcoded here or anywhere else in source. Entered manually per device via
// the Settings screen instead. Every sync feature (ytHistory.js today, more
// later) reads it through here rather than touching storage directly.
//
// Backed by StorageMap (map 'core', key 'sync-server-url') - see
// docs/local-storage.md.

import { StorageMap } from './storageMap.js'

const MAP_ID = 'core'
const KEY = 'sync-server-url'

const map = StorageMap.get(MAP_ID)

// In-memory cache, kept in sync on every setSyncServerUrl() call. Needed
// because ytHistory.js's sendHistoryBeacon() must read this
// *synchronously* during page unload - navigator.sendBeacon only works
// as a plain, non-awaited call made in direct response to the unload
// event, and StorageMap.get() is async-only by design (no exceptions,
// even for a fast/cached path). Kicked off immediately on module load
// (not lazily on first call), so the cache is warm well before any
// synchronous access might need it, even in a session that never visits
// Settings. Narrow, acceptable gap: a page closed extremely quickly after
// load, before this resolves, would beacon with an empty cached URL even
// if one was configured - unavoidable without blocking app startup on it.
let cachedUrl = ''
const readyPromise = (async () => {
  const value = await map.get(KEY)
  cachedUrl = typeof value === 'string' ? value : ''
})()

export async function getSyncServerUrl() {
  await readyPromise
  return cachedUrl
}

// Synchronous access to the cache, for contexts that can't await (see
// sendHistoryBeacon() in ytHistory.js). Returns '' if the initial load
// hasn't resolved yet.
export function getSyncServerUrlSync() {
  return cachedUrl
}

export async function setSyncServerUrl(url) {
  await readyPromise
  const trimmed = url.trim().replace(/\/+$/, '')
  cachedUrl = trimmed
  if (trimmed) {
    await map.set(KEY, trimmed)
  } else {
    await map.delete(KEY)
  }
}
