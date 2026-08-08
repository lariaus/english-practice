// The client's current local calendar date, as a plain "YYYY-MM-DD" string -
// see docs/flashcards-spec.md's "Dates, not timestamps" section for why this
// is a local date, not a UTC one or a timestamp: the server never derives
// "what day is it," it only ever trusts what the client states directly.

const TEST_DAY_STORAGE_KEY = 'test-fake-today'

function realToday() {
  const now = new Date()
  const month = String(now.getMonth() + 1).padStart(2, '0')
  const day = String(now.getDate()).padStart(2, '0')
  return `${now.getFullYear()}-${month}-${day}`
}

function addOneDay(dateStr) {
  const [year, month, day] = dateStr.split('-').map(Number)
  const next = new Date(year, month - 1, day + 1)
  const nextMonth = String(next.getMonth() + 1).padStart(2, '0')
  const nextDay = String(next.getDate()).padStart(2, '0')
  return `${next.getFullYear()}-${nextMonth}-${nextDay}`
}

// Set VITE_TEST_FLASHCARDS_REFRESH_DAY in a gitignored .env.local (see
// .gitignore's *.local rule) and rebuild to exercise Review's day-by-day
// scheduling without waiting for real days to pass: instead of the real
// date, every page load advances a fake "today" (kept in localStorage - a
// deliberate hack, not routed through StorageMap) by exactly one calendar
// day. Cached after the first today() call in a session so every
// subsequent call that page load agrees, rather than advancing again.
let cachedFakeToday = null

export function today() {
  if (!import.meta.env.VITE_TEST_FLASHCARDS_REFRESH_DAY) {
    return realToday()
  }

  if (cachedFakeToday === null) {
    const stored = localStorage.getItem(TEST_DAY_STORAGE_KEY)
    cachedFakeToday = stored ? addOneDay(stored) : realToday()
    localStorage.setItem(TEST_DAY_STORAGE_KEY, cachedFakeToday)
  }
  return cachedFakeToday
}
