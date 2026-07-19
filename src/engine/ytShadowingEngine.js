// Framework-agnostic engine for the YT Shadowing tool.
//
// Wraps the YouTube IFrame Player API: lazy-loads the API script once (it
// attaches itself to the page via a global callback, not an ES import), and
// creates/reuses a YT.Player bound to a DOM element id. Loads a video,
// autoplays it, and exposes transport controls (play/pause, seek by a
// relative offset, playback rate) plus a polled start/end segment loop.

export const SEEK_STEP_SECONDS = 5
export const MIN_PLAYBACK_RATE = 0.5
export const MAX_PLAYBACK_RATE = 2.0
export const PLAYBACK_RATE_STEP = 0.05
const LOOP_POLL_MS = 50

let apiReadyPromise = null

function loadYouTubeIframeAPI() {
  if (apiReadyPromise) return apiReadyPromise

  apiReadyPromise = new Promise((resolve) => {
    if (window.YT && window.YT.Player) {
      resolve(window.YT)
      return
    }

    const previousCallback = window.onYouTubeIframeAPIReady
    window.onYouTubeIframeAPIReady = () => {
      previousCallback?.()
      resolve(window.YT)
    }

    const tag = document.createElement('script')
    tag.src = 'https://www.youtube.com/iframe_api'
    document.head.appendChild(tag)
  })

  return apiReadyPromise
}

// Accepts a full YouTube URL (watch/shorts/youtu.be/embed) or a bare 11-char
// video ID, returns the video ID or null if it can't be parsed.
export function parseYouTubeVideoId(input) {
  const trimmed = (input || '').trim()
  if (!trimmed) return null

  if (/^[\w-]{11}$/.test(trimmed)) return trimmed

  let url
  try {
    url = new URL(trimmed)
  } catch {
    return null
  }

  if (url.hostname === 'youtu.be') {
    const id = url.pathname.slice(1)
    return /^[\w-]{11}$/.test(id) ? id : null
  }

  if (url.hostname.includes('youtube.com')) {
    const vParam = url.searchParams.get('v')
    if (vParam && /^[\w-]{11}$/.test(vParam)) return vParam

    const match = url.pathname.match(/\/(?:embed|shorts)\/([\w-]{11})/)
    if (match) return match[1]
  }

  return null
}

export class YtShadowingEngine {
  constructor({ onChange } = {}) {
    this.onChange = onChange
    this.phase = 'idle' // idle | loading | ready | error
    this.error = null
    this.isPlaying = false
    this.playbackRate = 1
    this.videoTitle = null
    this._player = null
    this._loopRange = null
    this._loopInterval = null

    this._emit()
  }

  async loadVideo(elementId, videoId) {
    this.error = null
    this.videoTitle = null
    this._setPhase('loading')

    let YT
    try {
      YT = await loadYouTubeIframeAPI()
    } catch {
      this.error = 'Could not load the YouTube player.'
      this._setPhase('error')
      return
    }

    if (this._player) {
      this._player.loadVideoById(videoId)
      this._setPhase('ready')
      return
    }

    // Built manually (rather than letting YT.Player() create the iframe
    // from the placeholder div) so `referrerPolicy` can be set on it
    // directly, before its `src` navigates - a page-wide `no-referrer` meta
    // tag suppresses the Referer header YouTube's embed validation wants
    // (see index.html), causing Error 153 in Safari specifically, and an
    // element-level policy takes precedence over that page-wide one.
    const iframe = document.createElement('iframe')
    iframe.id = elementId
    iframe.referrerPolicy = 'strict-origin-when-cross-origin'
    iframe.allow = 'autoplay; encrypted-media; picture-in-picture'
    iframe.allowFullscreen = true
    const embedParams = new URLSearchParams({
      enablejsapi: '1',
      autoplay: '1',
      playsinline: '1',
      origin: window.location.origin,
    })
    iframe.src = `https://www.youtube.com/embed/${videoId}?${embedParams.toString()}`
    document.getElementById(elementId)?.replaceWith(iframe)

    await new Promise((resolve) => {
      this._player = new YT.Player(iframe, {
        events: {
          onReady: (event) => {
            event.target.playVideo()
            this.playbackRate = event.target.getPlaybackRate() || 1
            resolve()
          },
          onStateChange: (event) => {
            this.isPlaying = event.data === YT.PlayerState.PLAYING
            if (!this.videoTitle) {
              const data = this._player.getVideoData ? this._player.getVideoData() : null
              if (data?.title) this.videoTitle = data.title
            }
            this._emit()
          },
          onPlaybackRateChange: (event) => {
            this.playbackRate = event.data
            this._emit()
          },
          onError: () => {
            this.error = 'That video could not be played (invalid ID or restricted from embedding).'
            this._setPhase('error')
            resolve()
          },
        },
      })
    })

    if (this.phase !== 'error') this._setPhase('ready')
  }

  togglePlayPause() {
    if (!this._player) return
    if (this.isPlaying) {
      this._player.pauseVideo()
    } else {
      this._player.playVideo()
    }
  }

  play() {
    this._player?.playVideo()
  }

  pause() {
    this._player?.pauseVideo()
  }

  getCurrentTime() {
    return this._player?.getCurrentTime ? this._player.getCurrentTime() : 0
  }

  getDuration() {
    return this._player?.getDuration ? this._player.getDuration() : 0
  }

  seekTo(seconds) {
    if (!this._player?.seekTo) return
    const duration = this._player.getDuration ? this._player.getDuration() : 0
    const max = duration > 0 ? duration : Infinity
    const target = Math.max(0, Math.min(max, seconds))
    this._player.seekTo(target, true)
  }

  seekBy(deltaSeconds) {
    if (!this._player?.getCurrentTime) return
    this.seekTo(this._player.getCurrentTime() + deltaSeconds)
  }

  // Arms the start..end loop-poller (jumps back to start whenever playback
  // reaches/passes end) without seeking or changing play state - used to
  // (re-)enter loop mode while leaving the player exactly as it is.
  setLoopRange(startSeconds, endSeconds) {
    this._loopRange = { start: startSeconds, end: endSeconds }
    this._stopLoopPolling()
    this._loopInterval = setInterval(() => {
      if (!this._loopRange || !this._player?.getCurrentTime) return
      if (this._player.getCurrentTime() >= this._loopRange.end) {
        this.seekTo(this._loopRange.start)
      }
    }, LOOP_POLL_MS)
  }

  // Plays start..end on repeat: seeks to start and plays, then polls
  // getCurrentTime() (the IFrame API has no native "stop at time X" event)
  // and jumps back to start whenever playback reaches/passes end - whether
  // that's from natural playback or a manual seek past the boundary.
  startLoop(startSeconds, endSeconds) {
    if (!this._player) return
    this.setLoopRange(startSeconds, endSeconds)
    this.seekTo(startSeconds)
    this._player.playVideo()
  }

  stopLoop() {
    this._stopLoopPolling()
    this._loopRange = null
  }

  // Plays start..end once (not on repeat) and resolves once playback
  // reaches end, leaving it paused there. Used by Shadow's per-cycle
  // "play the full capture" step, as opposed to startLoop()'s continuous
  // auto-repeat.
  async playRangeOnce(startSeconds, endSeconds) {
    if (!this._player) return
    this.seekTo(startSeconds)
    this._player.playVideo()

    await new Promise((resolve) => {
      const interval = setInterval(() => {
        if (!this._player?.getCurrentTime) return
        if (this._player.getCurrentTime() >= endSeconds) {
          clearInterval(interval)
          resolve()
        }
      }, LOOP_POLL_MS)
    })

    this.pause()
  }

  _stopLoopPolling() {
    if (this._loopInterval) {
      clearInterval(this._loopInterval)
      this._loopInterval = null
    }
  }

  setPlaybackRate(rate) {
    if (!this._player?.setPlaybackRate) return
    const clamped = Math.min(MAX_PLAYBACK_RATE, Math.max(MIN_PLAYBACK_RATE, rate))
    this._player.setPlaybackRate(clamped)
    // YouTube snaps to its own supported rate list, so read back what
    // actually applied rather than trusting the requested value verbatim.
    this.playbackRate = this._player.getPlaybackRate()
    this._emit()
  }

  changeSpeed(deltaRate) {
    this.setPlaybackRate(this.playbackRate + deltaRate)
  }

  destroy() {
    this._stopLoopPolling()
    this._loopRange = null
    if (this._player?.destroy) {
      try {
        this._player.destroy()
      } catch {
        // ignore - player already gone
      }
    }
    this._player = null
  }

  _setPhase(phase) {
    this.phase = phase
    this._emit()
  }

  _emit() {
    if (!this.onChange) return
    this.onChange({
      phase: this.phase,
      error: this.error,
      isPlaying: this.isPlaying,
      playbackRate: this.playbackRate,
      videoTitle: this.videoTitle,
    })
  }
}
