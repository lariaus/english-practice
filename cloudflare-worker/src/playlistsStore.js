// KV storage layer for Playlists - see docs/yt-shadowing-spec.md's
// "Playlists" section. One KV key per playlist (keyed by name), plus one
// small index key holding `{name, updatedAt}` for every playlist, so
// list_playlists() can return a last-edited-sorted list without reading
// every playlist's full video array (richer than Flashcards' plain-name
// index, which doesn't need to support sorting). Both live in the
// PLAYLISTS namespace, kept separate from HISTORY/FLASHCARDS so each
// feature's per-key write-rate limits never compete with each other.
//
// Every mutation writes the playlist's own key BEFORE touching the index -
// if the index write then fails, the playlist just shows a stale sort
// position (self-heals on the next successful mutation) rather than the
// reverse: a playlist jumping to the top of "last edited" with no actual
// content change, a much more confusing failure to chase later.

const INDEX_KEY = 'playlists:index'

function playlistKey(name) {
  return `playlist:${name}`
}

export async function getIndex(env) {
  return (await env.PLAYLISTS.get(INDEX_KEY, 'json')) ?? []
}

async function putIndex(env, index) {
  await env.PLAYLISTS.put(INDEX_KEY, JSON.stringify(index))
}

// Upserts one {name, updatedAt} entry - replaces any existing entry for
// that name. Used both for ordinary content-changed bumps and, during a
// rename, to add the new name's entry.
export async function upsertIndexEntry(env, name, updatedAt) {
  const index = await getIndex(env)
  const withoutExisting = index.filter((entry) => entry.name !== name)
  await putIndex(env, [...withoutExisting, { name, updatedAt }])
}

export async function removeIndexEntry(env, name) {
  const index = await getIndex(env)
  await putIndex(env, index.filter((entry) => entry.name !== name))
}

export async function getPlaylist(env, name) {
  return await env.PLAYLISTS.get(playlistKey(name), 'json')
}

export async function putPlaylist(env, name, playlist) {
  await env.PLAYLISTS.put(playlistKey(name), JSON.stringify(playlist))
}

export async function deletePlaylist(env, name) {
  await env.PLAYLISTS.delete(playlistKey(name))
}
