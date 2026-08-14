// Direct tests of the storage module itself - calling putPlaylist/
// getPlaylist/upsertIndexEntry against the real KV binding directly, with
// no HTTP request/response or JSON body parsing involved at all.
// Complements playlistsRoutes.test.js's full API-level round-trips - see
// flashcardsStore.test.js's matching comment for the same reasoning.
import { env } from 'cloudflare:test'
import { describe, expect, it } from 'vitest'
import { getIndex, getPlaylist, putPlaylist, removeIndexEntry, upsertIndexEntry } from '../src/playlistsStore.js'

let playlistCounter = 0
function uniquePlaylistName() {
  playlistCounter += 1
  return `direct-test-playlist-${playlistCounter}`
}

describe('playlistsStore - direct KV round-trips', () => {
  it('putPlaylist/getPlaylist round-trips a playlist exactly, including Unicode video titles', async () => {
    const name = uniquePlaylistName()
    const playlist = {
      name,
      updatedAt: 1234,
      videos: [
        {
          videoId: 'abc123',
          url: 'https://www.youtube.com/watch?v=abc123',
          title: '日本語のレッスン - déjà vu', // Japanese + French accents in one title
          author: 'Some Channel',
          duration: 300,
        },
      ],
    }

    await putPlaylist(env, name, playlist)
    const loaded = await getPlaylist(env, name)

    expect(loaded).toEqual(playlist)
  })

  it('getPlaylist returns null for a playlist that was never put', async () => {
    expect(await getPlaylist(env, uniquePlaylistName())).toBeNull()
  })

  it('upsertIndexEntry adds a new entry and getIndex round-trips a Unicode name exactly', async () => {
    const name = `日本語-${uniquePlaylistName()}`
    await upsertIndexEntry(env, name, 1000)

    const index = await getIndex(env)
    const entry = index.find((e) => e.name === name)
    expect(entry).toEqual({ name, updatedAt: 1000 })
  })

  it('upsertIndexEntry replaces an existing entry for the same name rather than duplicating it', async () => {
    const name = uniquePlaylistName()
    await upsertIndexEntry(env, name, 1000)
    await upsertIndexEntry(env, name, 2000)

    const index = await getIndex(env)
    const matches = index.filter((e) => e.name === name)
    expect(matches).toEqual([{ name, updatedAt: 2000 }])
  })

  it('removeIndexEntry removes only the named entry', async () => {
    const keep = uniquePlaylistName()
    const remove = uniquePlaylistName()
    await upsertIndexEntry(env, keep, 1000)
    await upsertIndexEntry(env, remove, 1000)

    await removeIndexEntry(env, remove)

    const index = await getIndex(env)
    expect(index.find((e) => e.name === remove)).toBeUndefined()
    expect(index.find((e) => e.name === keep)).toEqual({ name: keep, updatedAt: 1000 })
  })
})
