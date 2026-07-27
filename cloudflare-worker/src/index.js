// Cloudflare Worker backing the app's cross-device sync (see
// cloudflare-worker/README.md for setup). First use case: replaces
// ytHistory.js's localStorage-backed "last 5 videos" list with the same
// data living in one shared KV entry instead, so it's the same on every
// device rather than per-browser.
//
// No auth check - this Worker's URL is deliberately never committed to the
// repo/source (entered manually per device instead, see the app's settings
// screen), so the URL itself is the only thing gating access. Acceptable
// here specifically because the data involved isn't sensitive, and because
// Cloudflare's free tier rejects requests once the daily quota is used
// rather than billing for overage - so even abuse of a leaked URL can't
// turn into a surprise bill unless this account is later upgraded to a paid
// plan.

const HISTORY_LIMIT = 5
const HISTORY_KEY = 'history'

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Authorization, Content-Type',
}

function jsonResponse(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
  })
}

// Moves videoId to the front if already present (no duplicates), trims to
// the last HISTORY_LIMIT entries - same logic as the client's own
// addToHistory() did against localStorage, just running here instead.
//
// currentPosition is the one field that's deliberately protected: the
// client only ever sends a real number for it when a watch session ends
// (Back, or page close/reload) - the write that fires when a video starts
// omits it entirely, meaning "don't touch this," not "reset it to zero."
// Everything else (title/author/duration/url) is always sent fresh and
// just overwrites outright.
function withEntryAddedToFront(history, entry) {
  const existing = history.find((item) => item.videoId === entry.videoId)
  const withoutExisting = history.filter((item) => item.videoId !== entry.videoId)
  const merged = {
    ...entry,
    currentPosition:
      typeof entry.currentPosition === 'number' ? entry.currentPosition : (existing?.currentPosition ?? 0),
  }
  return [merged, ...withoutExisting].slice(0, HISTORY_LIMIT)
}

export default {
  async fetch(request, env) {
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: CORS_HEADERS })
    }

    const url = new URL(request.url)

    if (url.pathname === '/history' && request.method === 'GET') {
      const history = (await env.HISTORY.get(HISTORY_KEY, 'json')) ?? []
      return jsonResponse(history)
    }

    if (url.pathname === '/history' && request.method === 'POST') {
      const body = await request.json().catch(() => null)
      if (!body?.videoId || !body?.url || !body?.title) {
        return jsonResponse({ error: 'videoId, url, and title are required' }, 400)
      }

      const current = (await env.HISTORY.get(HISTORY_KEY, 'json')) ?? []
      const updated = withEntryAddedToFront(current, {
        videoId: body.videoId,
        url: body.url,
        title: body.title,
        author: body.author ?? null,
        duration: typeof body.duration === 'number' ? body.duration : 0,
        currentPosition: body.currentPosition,
      })

      await env.HISTORY.put(HISTORY_KEY, JSON.stringify(updated))
      // Return the value just computed rather than re-reading it from KV -
      // KV is only eventually consistent, so a read-after-write here could
      // race and return stale data.
      return jsonResponse(updated)
    }

    return jsonResponse({ error: 'Not found' }, 404)
  },
}
