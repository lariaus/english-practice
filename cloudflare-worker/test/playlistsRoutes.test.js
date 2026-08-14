// Miniflare integration tests - these actually touch KV/HTTP, unlike
// ankiScheduler.test.js's plain unit tests. Each test uses its own unique
// playlist name, mirroring flashcardsRoutes.test.js's convention.
import { SELF } from 'cloudflare:test'
import { describe, expect, it } from 'vitest'

let playlistCounter = 0
function uniquePlaylistName() {
  playlistCounter += 1
  return `test-playlist-${playlistCounter}`
}

async function createPlaylist(name) {
  return SELF.fetch('https://example.com/playlists', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name }),
  })
}

async function addVideo(name, video = {}) {
  const response = await SELF.fetch(`https://example.com/playlists/${encodeURIComponent(name)}/videos`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      videoId: 'vid1',
      url: 'https://www.youtube.com/watch?v=vid1',
      title: 'Some Video',
      ...video,
    }),
  })
  return response.json()
}

describe('list_playlists / create_playlist / get_playlist / delete_playlist', () => {
  it('create_playlist returns the new empty playlist', async () => {
    const name = uniquePlaylistName()
    const response = await createPlaylist(name)
    expect(response.status).toBe(200)
    const body = await response.json()
    expect(body.name).toBe(name)
    expect(body.videos).toEqual([])
    expect(typeof body.updatedAt).toBe('number')
  })

  it('create_playlist errors on a name collision', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)
    const response = await createPlaylist(name)
    expect(response.status).toBe(409)
  })

  it('get_playlist 404s for an unknown playlist', async () => {
    const response = await SELF.fetch(`https://example.com/playlists/${uniquePlaylistName()}`)
    expect(response.status).toBe(404)
  })

  it('delete_playlist removes it from both storage and the index', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)

    const deleteResponse = await SELF.fetch(`https://example.com/playlists/${name}`, { method: 'DELETE' })
    expect(deleteResponse.status).toBe(200)

    expect((await SELF.fetch(`https://example.com/playlists/${name}`)).status).toBe(404)
    const index = await (await SELF.fetch('https://example.com/playlists')).json()
    expect(index.find((e) => e.name === name)).toBeUndefined()
  })

  it('delete_playlist 404s for an unknown playlist', async () => {
    const response = await SELF.fetch(`https://example.com/playlists/${uniquePlaylistName()}`, { method: 'DELETE' })
    expect(response.status).toBe(404)
  })

  it('list_playlists sorts by updatedAt descending', async () => {
    const older = uniquePlaylistName()
    const newer = uniquePlaylistName()
    await createPlaylist(older)
    await createPlaylist(newer)
    // Touch `older` again so it becomes the most recently updated.
    await addVideo(older)

    const index = await (await SELF.fetch('https://example.com/playlists')).json()
    const olderPos = index.findIndex((e) => e.name === older)
    const newerPos = index.findIndex((e) => e.name === newer)
    expect(olderPos).toBeLessThan(newerPos)
  })

  it('round-trips a Unicode playlist name through the URL path', async () => {
    const name = `日本語-${uniquePlaylistName()}`
    const createResponse = await createPlaylist(name)
    expect(createResponse.status).toBe(200)

    const getResponse = await SELF.fetch(`https://example.com/playlists/${encodeURIComponent(name)}`)
    expect(getResponse.status).toBe(200)
    expect((await getResponse.json()).name).toBe(name)
  })
})

describe('rename_playlist', () => {
  it('renames a playlist, moving it to the new key and index entry', async () => {
    const name = uniquePlaylistName()
    const newName = uniquePlaylistName()
    await createPlaylist(name)

    const response = await SELF.fetch(`https://example.com/playlists/${name}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ newName }),
    })
    expect(response.status).toBe(200)
    expect((await response.json()).name).toBe(newName)

    expect((await SELF.fetch(`https://example.com/playlists/${name}`)).status).toBe(404)
    expect((await SELF.fetch(`https://example.com/playlists/${newName}`)).status).toBe(200)
  })

  it('renaming to the exact same name is a no-op success, not a 409', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)

    const response = await SELF.fetch(`https://example.com/playlists/${name}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ newName: name }),
    })
    expect(response.status).toBe(200)
    expect((await SELF.fetch(`https://example.com/playlists/${name}`)).status).toBe(200)
  })

  it('rename 409s if the new name is already taken', async () => {
    const name = uniquePlaylistName()
    const taken = uniquePlaylistName()
    await createPlaylist(name)
    await createPlaylist(taken)

    const response = await SELF.fetch(`https://example.com/playlists/${name}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ newName: taken }),
    })
    expect(response.status).toBe(409)
  })

  it('rename 404s if the playlist being renamed does not exist', async () => {
    const response = await SELF.fetch(`https://example.com/playlists/${uniquePlaylistName()}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ newName: uniquePlaylistName() }),
    })
    expect(response.status).toBe(404)
  })
})

describe('add_video / delete_video', () => {
  it('add_video prepends a video and returns added: true', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)

    const result = await addVideo(name, { videoId: 'vid1', title: 'First' })
    expect(result.added).toBe(true)
    expect(result.playlist.videos).toEqual([
      { videoId: 'vid1', url: 'https://www.youtube.com/watch?v=vid1', title: 'First', author: null, duration: 0 },
    ])

    const result2 = await addVideo(name, { videoId: 'vid2', title: 'Second' })
    expect(result2.playlist.videos.map((v) => v.videoId)).toEqual(['vid2', 'vid1'])
  })

  it('adding a video already in the playlist is a no-op: no reorder, no updatedAt bump', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)
    await addVideo(name, { videoId: 'vid1' })
    await addVideo(name, { videoId: 'vid2' })

    const before = await (await SELF.fetch(`https://example.com/playlists/${name}`)).json()
    const result = await addVideo(name, { videoId: 'vid1', title: 'Re-added' })

    expect(result.added).toBe(false)
    expect(result.playlist.videos.map((v) => v.videoId)).toEqual(['vid2', 'vid1'])
    expect(result.playlist.updatedAt).toBe(before.updatedAt)
  })

  it('add_video 400s when required fields are missing', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)

    const response = await SELF.fetch(`https://example.com/playlists/${name}/videos`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ videoId: 'vid1' }), // missing url/title
    })
    expect(response.status).toBe(400)
  })

  it('add_video 404s for an unknown playlist', async () => {
    const response = await SELF.fetch(`https://example.com/playlists/${uniquePlaylistName()}/videos`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ videoId: 'vid1', url: 'https://example.com', title: 'x' }),
    })
    expect(response.status).toBe(404)
  })

  it('delete_video removes just that video', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)
    await addVideo(name, { videoId: 'vid1' })
    await addVideo(name, { videoId: 'vid2' })

    const response = await SELF.fetch(`https://example.com/playlists/${name}/videos/vid1`, { method: 'DELETE' })
    expect(response.status).toBe(200)

    const playlist = await (await SELF.fetch(`https://example.com/playlists/${name}`)).json()
    expect(playlist.videos.map((v) => v.videoId)).toEqual(['vid2'])
  })

  it('delete_video 404s for an unknown playlist', async () => {
    const response = await SELF.fetch(`https://example.com/playlists/${uniquePlaylistName()}/videos/vid1`, {
      method: 'DELETE',
    })
    expect(response.status).toBe(404)
  })

  it('delete_video 404s for an unknown videoId within a real playlist', async () => {
    const name = uniquePlaylistName()
    await createPlaylist(name)
    await addVideo(name, { videoId: 'vid1' })

    const response = await SELF.fetch(`https://example.com/playlists/${name}/videos/does-not-exist`, {
      method: 'DELETE',
    })
    expect(response.status).toBe(404)
  })
})
