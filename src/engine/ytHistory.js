// YT Shadowing history: last N distinct videos watched, most-recent first -
// now backed by the Cloudflare Worker (see cloudflare-worker/) instead of
// localStorage, so it's shared across devices rather than per-browser.
// Best-effort only, same convention as nativeServerClient.js: every
// function here resolves to an empty/unchanged list on any failure (no sync
// server configured yet, network error, bad response) rather than throwing -
// history sync is never required for the app to work.

import { getSyncServerUrl, getSyncServerUrlSync } from './syncConfig.js'

export async function loadHistory() {
  const serverUrl = await getSyncServerUrl()
  if (!serverUrl) return []

  try {
    const response = await fetch(`${serverUrl}/history`)
    if (!response.ok) return []
    const data = await response.json()
    return Array.isArray(data) ? data : []
  } catch {
    return []
  }
}

// Moves videoId to the front if already present (no duplicates), trims to
// the last N entries - the actual dedupe/trim logic now lives on the Worker
// (see cloudflare-worker/src/index.js), this just calls it and returns
// whatever it hands back.
//
// `entry.currentPosition` is deliberately optional - omit it (as the caller
// does when a video starts) to leave whatever position was last recorded
// untouched; only pass a real number (as the caller does when a session
// ends) to actually update it. See the Worker's own withEntryAddedToFront().
export async function addToHistory(entry) {
  const serverUrl = await getSyncServerUrl()
  if (!serverUrl) return []

  try {
    const response = await fetch(`${serverUrl}/history`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(entry),
    })
    if (!response.ok) return []
    const data = await response.json()
    return Array.isArray(data) ? data : []
  } catch {
    return []
  }
}

// Same shape/purpose as addToHistory(), but for the moment a page is
// actually closing/reloading rather than a normal in-app navigation - a
// regular fetch() gets cancelled mid-flight once the page starts unloading,
// so this uses sendBeacon instead, which browsers guarantee gets delivered
// even as the page goes away. No response to read either way (sendBeacon
// doesn't expose one), so there's nothing to return - this is fire-and-
// forget by nature, not just by choice.
//
// Sent as a plain string rather than declaring Content-Type: application/
// json - JSON isn't one of the 3 CORS-safelisted content types, and
// sendBeacon can't perform the CORS preflight a non-safelisted type would
// need for a cross-origin request. A raw string body defaults to
// text/plain, which is safelisted; the Worker's request.json() parses the
// body text regardless of what Content-Type was actually declared.
export function sendHistoryBeacon(entry) {
  // Synchronous read - this function can't await (see syncConfig.js's
  // getSyncServerUrlSync() for why).
  const serverUrl = getSyncServerUrlSync()
  if (!serverUrl) return
  if (typeof navigator === 'undefined' || !navigator.sendBeacon) return

  navigator.sendBeacon(`${serverUrl}/history`, JSON.stringify(entry))
}
