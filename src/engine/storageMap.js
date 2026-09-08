// A key->JSON-value map, like localStorage but server-backed (native-
// server's /storage/maps/:mapId/:key) and JSON-native (no manual
// JSON.stringify/parse), with support for multiple independent named
// maps. See docs/local-storage.md.
//
// Always backed by native-server - every real usage pattern (the native
// Mac/iOS app, or the web app served locally via native_server_cli) has
// native-server serving the page in the first place, so it's always
// reachable.

import { log } from './appLog.js'

class StorageMapHandle {
  constructor(mapId) {
    this._mapId = mapId
  }

  // Returns the value (already parsed), or null if absent.
  async get(key) {
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
