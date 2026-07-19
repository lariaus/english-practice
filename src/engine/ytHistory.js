// Plain localStorage-backed history for YT Shadowing: last N distinct
// videos watched, most-recent first. Not reactive itself - callers re-read
// with loadHistory() whenever they need a fresh snapshot (e.g. on screen
// mount), and write through addToHistory() after a load.

const HISTORY_STORAGE_KEY = 'yt-shadowing-history'
const HISTORY_LIMIT = 5

export function loadHistory() {
  try {
    const raw = localStorage.getItem(HISTORY_STORAGE_KEY)
    const parsed = raw ? JSON.parse(raw) : []
    return Array.isArray(parsed) ? parsed : []
  } catch {
    return []
  }
}

function saveHistory(history) {
  try {
    localStorage.setItem(HISTORY_STORAGE_KEY, JSON.stringify(history))
  } catch {
    // ignore - e.g. storage disabled or full
  }
}

// Moves videoId to the front if already present (no duplicates), trims to
// the last HISTORY_LIMIT entries, persists, and returns the new list.
export function addToHistory(videoId, url, title) {
  const withoutExisting = loadHistory().filter((entry) => entry.videoId !== videoId)
  const history = [{ videoId, url, title }, ...withoutExisting].slice(0, HISTORY_LIMIT)
  saveHistory(history)
  return history
}
