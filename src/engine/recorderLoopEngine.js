// Framework-agnostic engine for the Recorder Loop tool.
//
// Owns the beep -> record -> beep -> playback -> ... state machine, mic/
// stream lifecycle, MediaRecorder setup, and Wake Lock. Has no dependency
// on Vue: it reports state via a plain onChange(snapshot) callback so any
// view layer can bind to it.
//
// Both the transition beeps and the recorded-segment playback are scheduled
// through a single AudioContext that's created/resumed synchronously inside
// the Start button's click handler (see start()). Once that context is
// running, further scheduled playback through it doesn't require its own
// user gesture — unlike a plain `new Audio().play()` called later from a
// timer, which Safari silently blocks as unrequested autoplay.

const COUNTDOWN_TICK_MS = 200
const BEEP_DURATION_MS = 150
const BEEP_FREQUENCY_HZ = 880

const MIME_TYPE_CANDIDATES = [
  'audio/mp4', // Safari (iOS/macOS)
  'audio/webm;codecs=opus',
  'audio/webm',
  'audio/ogg;codecs=opus',
]

function pickSupportedMimeType() {
  if (typeof MediaRecorder === 'undefined' || !MediaRecorder.isTypeSupported) {
    return ''
  }
  return MIME_TYPE_CANDIDATES.find((type) => MediaRecorder.isTypeSupported(type)) || ''
}

export class RecorderLoopEngine {
  constructor(durationSeconds, { onChange } = {}) {
    this.durationSeconds = durationSeconds
    this.onChange = onChange

    this.phase = 'idle' // idle | requesting-permission | recording | playing | error
    this.secondsRemaining = durationSeconds
    this.error = null

    this.stream = null
    this.mimeType = ''
    this._stopRequested = false
    this._lastBlob = null

    this._audioCtx = null
    this._activeRecorder = null
    this._activeOscillator = null
    this._activeSourceNode = null
    this._activeResolve = null
    this._tickInterval = null
    this._wakeLock = null

    this._onVisibilityChange = () => {
      if (document.visibilityState === 'visible' && this.stream && !this._stopRequested) {
        this._acquireWakeLock()
      }
    }

    this._emit()
  }

  async start() {
    if (this.phase !== 'idle' && this.phase !== 'error') return

    if (typeof navigator === 'undefined' || !navigator.mediaDevices?.getUserMedia) {
      this.error = 'Microphone access is not supported in this browser.'
      this._setPhase('error')
      return
    }
    if (typeof MediaRecorder === 'undefined') {
      this.error = 'Audio recording is not supported in this browser.'
      this._setPhase('error')
      return
    }

    this.error = null
    this._stopRequested = false
    this._setPhase('requesting-permission')

    // Create/resume the AudioContext synchronously within this click
    // handler's call stack (before any `await`) so iOS Safari treats all
    // audio scheduled through it later as already-unlocked, not blocked
    // autoplay.
    this._ensureAudioContext()

    let stream
    try {
      stream = await navigator.mediaDevices.getUserMedia({ audio: true })
    } catch {
      this.error = 'Microphone access was denied or is unavailable.'
      this._setPhase('error')
      return
    }

    this.stream = stream
    this.mimeType = pickSupportedMimeType()

    await this._acquireWakeLock()
    document.addEventListener('visibilitychange', this._onVisibilityChange)

    this._loop()
  }

  stop() {
    if (this.phase === 'idle') return

    this._stopRequested = true
    this._stopCountdown()

    if (this._activeRecorder && this._activeRecorder.state !== 'inactive') {
      try {
        this._activeRecorder.stop()
      } catch {
        // ignore - recorder already stopping/stopped
      }
    }
    if (this._activeOscillator) {
      try {
        this._activeOscillator.stop()
      } catch {
        // ignore
      }
      this._activeOscillator = null
    }
    if (this._activeSourceNode) {
      try {
        this._activeSourceNode.stop()
      } catch {
        // ignore
      }
      this._activeSourceNode = null
    }

    const resolve = this._activeResolve
    this._activeResolve = null
    if (resolve) resolve()

    this._releaseWakeLock()
    document.removeEventListener('visibilitychange', this._onVisibilityChange)

    if (this.stream) {
      this.stream.getTracks().forEach((track) => track.stop())
      this.stream = null
    }
    if (this._audioCtx) {
      this._audioCtx.close().catch(() => {})
      this._audioCtx = null
    }

    this._setPhase(this.phase === 'error' ? 'error' : 'idle')
  }

  destroy() {
    this.stop()
  }

  async _loop() {
    while (!this._stopRequested) {
      await this._recordPhase()
      if (this._stopRequested) break
      await this._playPhase()
    }
  }

  async _recordPhase() {
    this.secondsRemaining = this.durationSeconds
    this._setPhase('recording')

    await this._playBeep()
    if (this._stopRequested) return

    await new Promise((resolve) => {
      const chunks = []
      let recorder
      try {
        recorder = this.mimeType
          ? new MediaRecorder(this.stream, { mimeType: this.mimeType })
          : new MediaRecorder(this.stream)
      } catch {
        this.error = 'Recording is not supported in this browser.'
        this._setPhase('error')
        resolve()
        return
      }

      this._activeRecorder = recorder
      this._activeResolve = resolve

      recorder.ondataavailable = (event) => {
        if (event.data && event.data.size > 0) chunks.push(event.data)
      }

      recorder.onstop = () => {
        this._activeRecorder = null
        if (!this._stopRequested) {
          this._lastBlob = chunks.length > 0
            ? new Blob(chunks, { type: recorder.mimeType || this.mimeType || 'audio/webm' })
            : null
        }
        const resolveFn = this._activeResolve
        this._activeResolve = null
        if (resolveFn) resolveFn()
      }

      recorder.start()
      this._startCountdown(() => {
        if (recorder.state !== 'inactive') recorder.stop()
      })
    })
  }

  async _playPhase() {
    if (!this._lastBlob) return

    this.secondsRemaining = this.durationSeconds
    this._setPhase('playing')

    await this._playBeep()
    if (this._stopRequested) return

    let audioBuffer
    try {
      const arrayBuffer = await this._lastBlob.arrayBuffer()
      if (this._stopRequested) return
      audioBuffer = await this._audioCtx.decodeAudioData(arrayBuffer)
    } catch {
      return // couldn't decode this segment - skip playback, loop continues
    }
    if (this._stopRequested) return

    await new Promise((resolve) => {
      const source = this._audioCtx.createBufferSource()
      source.buffer = audioBuffer
      source.connect(this._audioCtx.destination)

      this._activeSourceNode = source
      this._activeResolve = resolve

      source.onended = () => {
        this._stopCountdown()
        this._activeSourceNode = null
        const resolveFn = this._activeResolve
        this._activeResolve = null
        if (resolveFn) resolveFn()
      }

      source.start()
      this._startCountdown(() => {})
    })
  }

  _playBeep() {
    return new Promise((resolve) => {
      if (!this._audioCtx) {
        resolve()
        return
      }

      const ctx = this._audioCtx
      const oscillator = ctx.createOscillator()
      const gain = ctx.createGain()
      const now = ctx.currentTime

      oscillator.type = 'sine'
      oscillator.frequency.value = BEEP_FREQUENCY_HZ
      gain.gain.setValueAtTime(0.0001, now)
      gain.gain.exponentialRampToValueAtTime(0.25, now + 0.01)
      gain.gain.exponentialRampToValueAtTime(0.0001, now + BEEP_DURATION_MS / 1000)

      oscillator.connect(gain)
      gain.connect(ctx.destination)

      this._activeOscillator = oscillator
      this._activeResolve = resolve

      oscillator.onended = () => {
        this._activeOscillator = null
        const resolveFn = this._activeResolve
        this._activeResolve = null
        if (resolveFn) resolveFn()
      }

      oscillator.start(now)
      oscillator.stop(now + BEEP_DURATION_MS / 1000)
    })
  }

  _startCountdown(onComplete) {
    this._stopCountdown()
    const durationMs = this.durationSeconds * 1000
    const startedAt = Date.now()
    this.secondsRemaining = this.durationSeconds
    this._emit()

    this._tickInterval = setInterval(() => {
      const elapsed = Date.now() - startedAt
      const remaining = Math.max(0, durationMs - elapsed)
      this.secondsRemaining = Math.ceil(remaining / 1000)
      this._emit()
      if (elapsed >= durationMs) {
        this._stopCountdown()
        onComplete()
      }
    }, COUNTDOWN_TICK_MS)
  }

  _stopCountdown() {
    if (this._tickInterval) {
      clearInterval(this._tickInterval)
      this._tickInterval = null
    }
  }

  _ensureAudioContext() {
    if (!this._audioCtx) {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext
      if (!AudioContextClass) return
      this._audioCtx = new AudioContextClass()
    }
    if (this._audioCtx.state === 'suspended') {
      this._audioCtx.resume().catch(() => {})
    }
  }

  async _acquireWakeLock() {
    if (typeof navigator === 'undefined' || !('wakeLock' in navigator)) return
    try {
      this._wakeLock = await navigator.wakeLock.request('screen')
      this._wakeLock.addEventListener('release', () => {
        this._wakeLock = null
      })
    } catch {
      // Not fatal (e.g. Low Power Mode can reject the request).
    }
  }

  _releaseWakeLock() {
    if (this._wakeLock) {
      this._wakeLock.release().catch(() => {})
      this._wakeLock = null
    }
  }

  _setPhase(phase) {
    this.phase = phase
    this._emit()
  }

  _emit() {
    if (!this.onChange) return
    this.onChange({
      phase: this.phase,
      secondsRemaining: this.secondsRemaining,
      totalSeconds: this.durationSeconds,
      error: this.error,
    })
  }
}
