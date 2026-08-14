// Shared video-row formatting for YT Shadowing's History and Playlists
// panels (see docs/yt-shadowing-spec.md) - both need the exact same
// "Title - Author (duration) (progress%)" line, so it lives here once
// rather than being duplicated per screen.

export function formatTime(totalSeconds) {
  const safeSeconds = Number.isFinite(totalSeconds) ? Math.max(0, totalSeconds) : 0
  const minutes = Math.floor(safeSeconds / 60)
  const seconds = Math.floor(safeSeconds % 60)
  return `${minutes}:${String(seconds).padStart(2, '0')}`
}

const TITLE_MAX_LENGTH = 50

// Truncated explicitly in JS (not left to CSS's single-line ellipsis) so a
// long title can never crowd the author/duration/progress that follows it
// off the edge of the box - those would otherwise just get silently cut off
// along with the rest of an overflowing line.
export function truncateTitle(title) {
  if (title.length <= TITLE_MAX_LENGTH) return title
  return `${title.slice(0, TITLE_MAX_LENGTH)}...`
}

// "Title - Author (duration) (progress%)" - each trailing part only appears
// if that data exists (author/duration missing on older History entries
// written before those fields existed, or a Playlist video whose match
// isn't in History at all), and progress only shows once it's at least 1%
// (a fresh, barely-started entry would otherwise show a noisy "(0%)").
//
// Takes one object rather than positional args deliberately: a History
// entry has its own `currentPosition` built in and can be passed straight
// through, but a Playlist video never stores one (see
// docs/yt-shadowing-spec.md's "Playlists" section - position is always
// looked up live from History by videoId) - the caller assembles
// `duration`/`currentPosition` from whichever source is authoritative
// (History's, when the video's also in History, otherwise the playlist's
// own frozen copy) before calling this, rather than this function guessing.
export function formatVideoLine({ title, author, duration, currentPosition }) {
  let line = truncateTitle(title)
  if (author) line += ` - ${author}`
  if (duration) line += ` (${formatTime(duration)})`
  if (duration > 0) {
    const progressPercent = Math.round(((currentPosition || 0) / duration) * 100)
    if (progressPercent >= 1) line += ` (${progressPercent}%)`
  }
  return line
}
