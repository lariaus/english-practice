// Client for native-server's /data-packs/... endpoints - see
// docs/data-pack-sync.md. `listPacks`/`startSync` always target a specific
// remote `native-server` address (host:port, e.g. "192.168.1.5:8000") -
// there is no more "just list/sync from this same machine" mode; even a
// same-machine sync is done by pointing the address at this machine's own
// server. This device's own local server is the one making the outbound
// network call, over plain HTTP, C++-to-C++ - never a cross-origin request
// from this page itself.
//
// Deliberately relative paths, not an absolute URL with a hardcoded port -
// same reasoning as nativeServerClient.js: native-server serves this app's
// own static files too, so these are always same-origin as the page itself;
// only the `remote` query param, resolved server-side, ever points
// elsewhere.

import { log } from './appLog.js'

// Returns { ok: true, packs: [...] } or { ok: false, error } - unlike a
// plain "degrade to []" convention, this is now the de facto connectivity
// check for the address the user just typed in (there's no separate "test
// connection" button - see docs/data-pack-sync.md), so a genuine failure
// (bad address, remote unreachable, remote returned something unexpected)
// must be distinguishable from "reached it fine, it just has no packs".
export async function listPacks(remoteAddress) {
  try {
    const response = await fetch(`/data-packs?remote=${encodeURIComponent(remoteAddress)}`)
    if (!response.ok) {
      const body = await response.json().catch(() => null)
      const error = body?.error || `HTTP ${response.status}`
      log('[DataPacks] listPacks failed:', error)
      return { ok: false, error }
    }
    const data = await response.json()
    return { ok: true, packs: Array.isArray(data.packs) ? data.packs : [] }
  } catch (err) {
    log('[DataPacks] listPacks errored:', err.message)
    return { ok: false, error: err.message }
  }
}

// Returns { ok: true } or { ok: false, error }, unlike listPacks/getSyncStatus
// above - this is a user-triggered action, so the caller needs the actual
// failure reason (already running, missing address) to show back to the
// user, not a silently-swallowed no-op. An unknown pack or an unreachable
// remoteAddress no longer surfaces here - both cases now start
// successfully (`{ ok: true }`) and show up later via getSyncStatus()'s
// `error` field, once the background sync actually fails (see
// docs/data-pack-sync.md).
export async function startSync(name, remoteAddress) {
  try {
    const response = await fetch(
      `/data-packs/${encodeURIComponent(name)}/sync?remote=${encodeURIComponent(remoteAddress)}`,
      { method: 'POST' },
    )
    if (!response.ok) {
      const body = await response.json().catch(() => null)
      const error = body?.error || `HTTP ${response.status}`
      log('[DataPacks] startSync failed:', name, error)
      return { ok: false, error }
    }
    return { ok: true }
  } catch (err) {
    log('[DataPacks] startSync errored:', name, err.message)
    return { ok: false, error: err.message }
  }
}

// Returns { pack, total, copied, done, error } (error is null if none), or
// null on a request failure - the caller (DataPacksScreen's poll loop)
// treats a null the same as "try again next tick" rather than tearing down
// the poll over one transient failure.
export async function getSyncStatus() {
  try {
    const response = await fetch('/data-packs/sync-status')
    if (!response.ok) {
      log('[DataPacks] getSyncStatus failed:', response.status)
      return null
    }
    return await response.json()
  } catch (err) {
    log('[DataPacks] getSyncStatus errored:', err.message)
    return null
  }
}

// Returns the current size of server_data in bytes, or null on failure -
// refreshed only at moments that matter (page load, right after a
// sync/clear finishes), never polled continuously.
export async function getServerDataSize() {
  try {
    const response = await fetch('/data-packs/server-data-size')
    if (!response.ok) {
      log('[DataPacks] getServerDataSize failed:', response.status)
      return null
    }
    const data = await response.json()
    return typeof data.bytes === 'number' ? data.bytes : null
  } catch (err) {
    log('[DataPacks] getServerDataSize errored:', err.message)
    return null
  }
}

// Returns { ok: true } or { ok: false, error } - same reasoning as
// startSync above.
export async function clearServerData() {
  try {
    const response = await fetch('/data-packs/clear', { method: 'POST' })
    if (!response.ok) {
      const body = await response.json().catch(() => null)
      const error = body?.error || `HTTP ${response.status}`
      log('[DataPacks] clearServerData failed:', error)
      return { ok: false, error }
    }
    return { ok: true }
  } catch (err) {
    log('[DataPacks] clearServerData errored:', err.message)
    return { ok: false, error: err.message }
  }
}
