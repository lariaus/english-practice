// YT Shadowing Playlists' client - see docs/yt-shadowing-spec.md's
// "Playlists" section for the full API this wraps.
//
// Deliberately a hybrid of this app's two existing sync-client conventions,
// not a clean clone of either:
//   - listPlaylists() stays SILENT (best-effort, `[]` on any failure) like
//     ytHistory.js - the Playlists panel sits right next to History on the
//     same screen, and if the sync server is unreachable, History's own
//     section already just quietly doesn't render. An explicit error here
//     would look inconsistent right beside it, and this matches the
//     documented default philosophy for this storage tier
//     (docs/common-design-philosophy.md's "Online shared storage" -
//     Flashcards' throw-on-failure is the stated exception, not the rule).
//   - getPlaylist() and every mutation THROW, like flashcardsClient.js -
//     silently showing "0 videos" for a playlist the user knows they
//     populated is a worse, more specific false signal than the list panel
//     simply not appearing, and create/rename/delete/add/remove are all
//     explicit user actions that need real pass/fail feedback (toasts,
//     confirm dialogs).

import { getSyncServerUrl } from './syncConfig.js'

function offlineError(message) {
  const error = new Error(message)
  error.offline = true
  return error
}

async function request(path, options) {
  const serverUrl = await getSyncServerUrl()
  if (!serverUrl) {
    throw offlineError('No sync server configured - add one in Settings first.')
  }

  let response
  try {
    response = await fetch(`${serverUrl}${path}`, options)
  } catch {
    throw offlineError('Could not reach the sync server. Check your connection and the URL in Settings.')
  }

  const data = await response.json().catch(() => null)
  if (!response.ok) {
    throw new Error(data?.error || `Sync server error (${response.status})`)
  }
  return data
}

function jsonBody(body) {
  return { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }
}

// list[{name, updatedAt}], sorted by updatedAt descending - never throws.
export async function listPlaylists() {
  const serverUrl = await getSyncServerUrl()
  if (!serverUrl) return []
  try {
    const response = await fetch(`${serverUrl}/playlists`)
    if (!response.ok) return []
    const data = await response.json()
    return Array.isArray(data) ? data : []
  } catch {
    return []
  }
}

export function getPlaylist(name) {
  return request(`/playlists/${encodeURIComponent(name)}`)
}

export function createPlaylist(name) {
  return request('/playlists', jsonBody({ name }))
}

export function renamePlaylist(name, newName) {
  return request(`/playlists/${encodeURIComponent(name)}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ newName }),
  })
}

export function deletePlaylist(name) {
  return request(`/playlists/${encodeURIComponent(name)}`, { method: 'DELETE' })
}

// -> {playlist, added} - `added` is false (a no-op, not an error) if the
// video was already in this playlist.
export function addVideoToPlaylist(name, video) {
  return request(`/playlists/${encodeURIComponent(name)}/videos`, jsonBody(video))
}

export function removeVideoFromPlaylist(name, videoId) {
  return request(`/playlists/${encodeURIComponent(name)}/videos/${encodeURIComponent(videoId)}`, {
    method: 'DELETE',
  })
}
