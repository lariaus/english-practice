// A key->JSON-value map, like localStorage but server-backed (native-
// server's /storage/maps/:mapId/:key) and JSON-native (no manual
// JSON.stringify/parse), with support for multiple independent named
// maps. See docs/local-storage.md.
//
// Exactly one backing store is ever active at a time: if native-server
// is reachable (checked once, see nativeServerClient.js), every call
// goes over the network for the rest of the session - no client-side
// caching, no dual-write. Only when native-server isn't reachable at all
// does this fall back to localStorage instead, for the whole session.

import { log } from './appLog.js'
import { isNativeServerAvailable } from './nativeServerClient.js'

function localStorageKeyFor(mapId) {
  return `storage-map:${mapId}`
}

function readLocalStorageBlob(mapId) {
  try {
    const raw = localStorage.getItem(localStorageKeyFor(mapId))
    if (!raw) return {}
    const parsed = JSON.parse(raw)
    return typeof parsed === 'object' && parsed !== null ? parsed : {}
  } catch (err) {
    log('[StorageMap] localStorage read failed:', mapId, err.message)
    return {}
  }
}

function writeLocalStorageBlob(mapId, blob) {
  try {
    localStorage.setItem(localStorageKeyFor(mapId), JSON.stringify(blob))
  } catch (err) {
    log('[StorageMap] localStorage write failed:', mapId, err.message)
  }
}

class StorageMapHandle {
  constructor(mapId) {
    this._mapId = mapId
  }

  // Returns the value (already parsed), or null if absent.
  async get(key) {
    if (!(await isNativeServerAvailable())) {
      const blob = readLocalStorageBlob(this._mapId)
      return key in blob ? blob[key] : null
    }

    try {
      const response = await fetch(`/storage/maps/${this._mapId}/${key}`)
      if (response.status === 404) {
        return null
      }
      if (!response.ok) {
        log('[StorageMap] get failed:', this._mapId, key, response.status)
        return null
      }
      const data = await response.json()
      return data.value
    } catch (err) {
      log('[StorageMap] get errored:', this._mapId, key, err.message)
      return null
    }
  }

  async set(key, value) {
    if (!(await isNativeServerAvailable())) {
      const blob = readLocalStorageBlob(this._mapId)
      blob[key] = value
      writeLocalStorageBlob(this._mapId, blob)
      return
    }

    try {
      const response = await fetch(`/storage/maps/${this._mapId}/${key}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value }),
      })
      if (!response.ok) {
        log('[StorageMap] set failed:', this._mapId, key, response.status)
      }
    } catch (err) {
      log('[StorageMap] set errored:', this._mapId, key, err.message)
    }
  }

  async delete(key) {
    if (!(await isNativeServerAvailable())) {
      const blob = readLocalStorageBlob(this._mapId)
      delete blob[key]
      writeLocalStorageBlob(this._mapId, blob)
      return
    }

    try {
      const response = await fetch(`/storage/maps/${this._mapId}/${key}`, { method: 'DELETE' })
      if (!response.ok) {
        log('[StorageMap] delete failed:', this._mapId, key, response.status)
      }
    } catch (err) {
      log('[StorageMap] delete errored:', this._mapId, key, err.message)
    }
  }
}

export const StorageMap = {
  get(mapId) {
    return new StorageMapHandle(mapId)
  },
}
