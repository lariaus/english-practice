// Playlists' HTTP routes - see docs/yt-shadowing-spec.md's "Playlists"
// section for the full API this implements. Kept in its own module and
// mirrors flashcardsRoutes.js's shape closely (matchRoute dispatch, own
// local CORS_HEADERS/jsonResponse - duplicated per-file is this Worker's
// established convention, not accidental).

import {
  deletePlaylist,
  getIndex,
  getPlaylist,
  putPlaylist,
  removeIndexEntry,
  upsertIndexEntry,
} from './playlistsStore.js'

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
  'Access-Control-Allow-Headers': 'Authorization, Content-Type',
}

function jsonResponse(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
  })
}

const notFound = (message) => jsonResponse({ error: message }, 404)
const badRequest = (message) => jsonResponse({ error: message }, 400)

// Matches `/playlists/:name/videos/:videoId`-style patterns against a real
// pathname - same helper as flashcardsRoutes.js's matchRoute, duplicated
// rather than shared (see that file's own comment for why).
function matchRoute(pattern, pathname) {
  const patternParts = pattern.split('/').filter(Boolean)
  const pathParts = pathname.split('/').filter(Boolean)
  if (patternParts.length !== pathParts.length) return null

  const params = {}
  for (let i = 0; i < patternParts.length; i++) {
    const patternPart = patternParts[i]
    if (patternPart.startsWith(':')) {
      params[patternPart.slice(1)] = decodeURIComponent(pathParts[i])
    } else if (patternPart !== pathParts[i]) {
      return null
    }
  }
  return params
}

async function handleListPlaylists(env) {
  const index = await getIndex(env)
  return jsonResponse([...index].sort((a, b) => b.updatedAt - a.updatedAt))
}

async function handleCreatePlaylist(request, env) {
  const body = await request.json().catch(() => null)
  const name = body?.name
  if (typeof name !== 'string' || !name) {
    return badRequest('name is required')
  }
  if (await getPlaylist(env, name)) {
    return jsonResponse({ error: `playlist "${name}" already exists` }, 409)
  }

  const updatedAt = Date.now()
  const playlist = { name, videos: [], updatedAt }
  await putPlaylist(env, name, playlist)
  await upsertIndexEntry(env, name, updatedAt)
  return jsonResponse(playlist)
}

async function handleGetPlaylist(env, name) {
  const playlist = await getPlaylist(env, name)
  if (!playlist) return notFound(`playlist "${name}" not found`)
  return jsonResponse(playlist)
}

async function handleRenamePlaylist(request, env, name) {
  const playlist = await getPlaylist(env, name)
  if (!playlist) return notFound(`playlist "${name}" not found`)

  const body = await request.json().catch(() => null)
  const newName = body?.newName
  if (typeof newName !== 'string' || !newName) {
    return badRequest('newName is required')
  }
  if (newName === name) {
    // No-op rename (e.g. Save pressed without changing the text) - a naive
    // "does newName already exist" check below would otherwise find this
    // same playlist under its own current name and incorrectly 409.
    return jsonResponse(playlist)
  }
  if (await getPlaylist(env, newName)) {
    return jsonResponse({ error: `playlist "${newName}" already exists` }, 409)
  }

  const updatedAt = Date.now()
  const renamed = { ...playlist, name: newName, updatedAt }
  // Write the new key, update the index, THEN delete the old key - in that
  // order, a crash partway through leaves at worst a harmless orphaned
  // dead key under the old name, never a lost playlist.
  await putPlaylist(env, newName, renamed)
  await upsertIndexEntry(env, newName, updatedAt)
  await removeIndexEntry(env, name)
  await deletePlaylist(env, name)
  return jsonResponse(renamed)
}

async function handleDeletePlaylist(env, name) {
  const playlist = await getPlaylist(env, name)
  if (!playlist) return notFound(`playlist "${name}" not found`)
  await deletePlaylist(env, name)
  await removeIndexEntry(env, name)
  return jsonResponse({ status: 'ok' })
}

async function handleAddVideo(request, env, name) {
  const playlist = await getPlaylist(env, name)
  if (!playlist) return notFound(`playlist "${name}" not found`)

  const body = await request.json().catch(() => null)
  if (!body?.videoId || !body?.url || !body?.title) {
    return badRequest('videoId, url, and title are required')
  }

  if (playlist.videos.some((v) => v.videoId === body.videoId)) {
    // Already there - deliberately a no-op, not a reorder/refresh (see
    // docs/yt-shadowing-spec.md) - the client shows an "already in X" toast
    // instead of a success toast, and nothing here counts as an edit.
    return jsonResponse({ playlist, added: false })
  }

  const video = {
    videoId: body.videoId,
    url: body.url,
    title: body.title,
    author: body.author ?? null,
    duration: typeof body.duration === 'number' ? body.duration : 0,
  }
  playlist.videos = [video, ...playlist.videos]
  playlist.updatedAt = Date.now()
  await putPlaylist(env, name, playlist)
  await upsertIndexEntry(env, name, playlist.updatedAt)
  return jsonResponse({ playlist, added: true })
}

async function handleDeleteVideo(env, name, videoId) {
  const playlist = await getPlaylist(env, name)
  if (!playlist) return notFound(`playlist "${name}" not found`)

  const index = playlist.videos.findIndex((v) => v.videoId === videoId)
  if (index === -1) return notFound(`video ${videoId} not found in playlist "${name}"`)

  playlist.videos.splice(index, 1)
  playlist.updatedAt = Date.now()
  await putPlaylist(env, name, playlist)
  await upsertIndexEntry(env, name, playlist.updatedAt)
  return jsonResponse({ status: 'ok' })
}

// Returns a Response if `request` matched a Playlists route, or null if it
// didn't - so index.js can fall through to its own 404 for anything else.
export async function handlePlaylistsRequest(request, env, url) {
  const { pathname } = url
  const { method } = request

  if (pathname === '/playlists' && method === 'GET') {
    return handleListPlaylists(env)
  }
  if (pathname === '/playlists' && method === 'POST') {
    return handleCreatePlaylist(request, env)
  }

  let params

  if ((params = matchRoute('/playlists/:name', pathname))) {
    if (method === 'GET') return handleGetPlaylist(env, params.name)
    if (method === 'PUT') return handleRenamePlaylist(request, env, params.name)
    if (method === 'DELETE') return handleDeletePlaylist(env, params.name)
  }

  if ((params = matchRoute('/playlists/:name/videos', pathname)) && method === 'POST') {
    return handleAddVideo(request, env, params.name)
  }

  if ((params = matchRoute('/playlists/:name/videos/:videoId', pathname)) && method === 'DELETE') {
    return handleDeleteVideo(env, params.name, params.videoId)
  }

  return null
}
