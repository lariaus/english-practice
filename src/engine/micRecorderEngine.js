// Framework-agnostic engine for mic capture in YT Shadowing: the manual
// "Record" toggle and the "Shadow" auto-sequence both go through here.
//
// start()/stop() is a simple manual toggle (unlike Recorder Loop / Robot
// Shadowing's automatic countdown-driven cycles): start() begins capturing
// the mic, stop() ends capture and immediately plays the recording back.
// recordFor()/playBlob() are the lower-level primitives Shadow uses instead,
// since it needs a beep between "recording ended" and "playback starts."
// Both paths share the same mic stream/AudioContext lifecycle, mirroring
// recorderLoopEngine.js's mic/MediaRecorder/AudioContext-playback patterns.

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

export class MicRecorderEngine {
  constructor({ onChange } = {}) {
    this.onChange = onChange
    this.phase = 'idle' // idle | requesting-permission | recording | playing | error
    this.error = null

    this.stream = null
    this.mimeType = ''
    this._audioCtx = null
    this._audioCtxSuspect = false
    this._activeRecorder = null
    this._activeSourceNode = null
    this._activeOscillator = null
    this._lastBlob = null

    this._emit()
  }

  async start() {
    if (this.phase !== 'idle' && this.phase !== 'error') return

    this.error = null
    await this._ensureInRightState()
    this._setPhase('requesting-permission')

    const ready = await this._ensureStream()
    if (!ready) return

    const recorder = this._createRecorder()
    if (!recorder) return

    const chunks = []
    recorder.ondataavailable = (event) => {
      if (event.data && event.data.size > 0) chunks.push(event.data)
    }
    recorder.onstop = () => {
      this._activeRecorder = null
      this._lastBlob = chunks.length > 0
        ? new Blob(chunks, { type: recorder.mimeType || this.mimeType || 'audio/webm' })
        : null
      this.playBlob(this._lastBlob)
    }

    this._activeRecorder = recorder
    recorder.start()
    this._setPhase('recording')
  }

  stop() {
    if (this.phase !== 'recording') return
    if (this._activeRecorder && this._activeRecorder.state !== 'inactive') {
      try {
        this._activeRecorder.stop()
      } catch {
        // ignore - recorder already stopping/stopped
      }
    }
  }

  // Records for a fixed duration and resolves with the recorded Blob (or
  // null on failure) - unlike start()/stop(), it does NOT auto-play the
  // result, so a caller (Shadow) can play a beep first.
  async recordFor(durationSeconds) {
    if (this.phase !== 'idle' && this.phase !== 'error') return null

    this.error = null
    await this._ensureInRightState()
    this._setPhase('requesting-permission')

    const ready = await this._ensureStream()
    if (!ready) return null

    const recorder = this._createRecorder()
    if (!recorder) return null

    const chunks = []
    recorder.ondataavailable = (event) => {
      if (event.data && event.data.size > 0) chunks.push(event.data)
    }

    const blob = await new Promise((resolve) => {
      recorder.onstop = () => {
        this._activeRecorder = null
        const result = chunks.length > 0
          ? new Blob(chunks, { type: recorder.mimeType || this.mimeType || 'audio/webm' })
          : null
        this._lastBlob = result
        resolve(result)
      }

      this._activeRecorder = recorder
      recorder.start()
      this._setPhase('recording')

      setTimeout(() => {
        if (recorder.state !== 'inactive') recorder.stop()
      }, durationSeconds * 1000)
    })

    this._setPhase('idle')
    return blob
  }

  // Like recordFor(), but tap-to-stop instead of a fixed duration - resolves
  // with the recorded Blob (or null on failure) once stop() is called,
  // WITHOUT auto-playing it back. Needed whenever a caller wants to insert
  // something (a beep) between "recording stopped" and "hearing it back" -
  // start()/stop()'s own contract goes straight from one to the other with
  // no seam to work with. Ended via the same public stop() as start() - it
  // only calls recorder.stop(), so whichever onstop handler is currently
  // attached (this one, or start()'s auto-playing one) is what actually
  // runs.
  async recordUntilStopped() {
    if (this.phase !== 'idle' && this.phase !== 'error') return null

    this.error = null
    await this._ensureInRightState()
    this._setPhase('requesting-permission')

    const ready = await this._ensureStream()
    if (!ready) return null

    const recorder = this._createRecorder()
    if (!recorder) return null

    const chunks = []
    recorder.ondataavailable = (event) => {
      if (event.data && event.data.size > 0) chunks.push(event.data)
    }

    const blob = await new Promise((resolve) => {
      recorder.onstop = () => {
        this._activeRecorder = null
        const result = chunks.length > 0
          ? new Blob(chunks, { type: recorder.mimeType || this.mimeType || 'audio/webm' })
          : null
        this._lastBlob = result
        resolve(result)
      }

      this._activeRecorder = recorder
      recorder.start()
      this._setPhase('recording')
    })

    this._setPhase('idle')
    return blob
  }

  async playBlob(blob) {
    if (!blob) {
      this._setPhase('idle')
      return
    }

    this._setPhase('playing')

    await this._ensureInRightState()
    if (!this._audioCtx) {
      this._setPhase('idle')
      return
    }
    let audioBuffer
    try {
      const arrayBuffer = await blob.arrayBuffer()
      audioBuffer = await this._audioCtx.decodeAudioData(arrayBuffer)
    } catch {
      this._setPhase('idle')
      return
    }

    await new Promise((resolve) => {
      const source = this._audioCtx.createBufferSource()
      source.buffer = audioBuffer
      source.connect(this._audioCtx.destination)

      this._activeSourceNode = source
      let done = false
      const finish = () => {
        if (done) return
        done = true
        this._activeSourceNode = null
        resolve()
      }
      source.onended = finish
      source.start()

      // AudioContext.state can misreport 'running' even when the context is
      // wedged (confirmed: every check up to here - stream, decode, ctx.state
      // - reported perfectly healthy, yet onended never fired, leaving the UI
      // stuck on "L" forever). Without a bound, a wedged context hangs this
      // promise indefinitely. Give it the real duration plus a generous
      // margin, then give up and mark the context suspect so the next call
      // forces a real recreation instead of trusting `.state` again.
      setTimeout(() => {
        if (done) return
        this._audioCtxSuspect = true
        finish()
      }, audioBuffer.duration * 1000 + 2000)
    })

    this._setPhase('idle')
  }

  async playBeep() {
    await this._ensureInRightState()
    if (!this._audioCtx) return

    return new Promise((resolve) => {
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
      let done = false
      const finish = () => {
        if (done) return
        done = true
        this._activeOscillator = null
        resolve()
      }
      oscillator.onended = finish

      oscillator.start(now)
      oscillator.stop(now + BEEP_DURATION_MS / 1000)

      // Same wedged-context bound as playBlob() above - a beep that never
      // ends would otherwise hang whatever's awaiting it (Shadow's
      // play-beep-then-record sequence) forever.
      setTimeout(() => {
        if (done) return
        this._audioCtxSuspect = true
        finish()
      }, BEEP_DURATION_MS + 1000)
    })
  }

  // The single gate every mic/playback entry point above calls first:
  // catches the ways iOS leaves this engine's stream/context looking fine
  // but silently producing no audio after the app's been backgrounded a
  // while, so a fresh press recovers on its own instead of failing forever.
  // Checking on every use (rather than reacting to visibilitychange, like
  // recorderLoopEngine.js/robotShadowingEngine.js do for their wake lock)
  // matters here specifically because WKWebView's visibilitychange firing
  // on background/foreground is itself unreliable - this works regardless
  // of whether that event ever fires.
  async _ensureInRightState() {
    this._dropDeadStream()
    await this._ensureAudioContext()
  }

  _dropDeadStream() {
    if (!this.stream) return
    // iOS mutes mic capture at the OS level once a WKWebView backgrounds
    // (Control Center's "Mic Mode" flips to "Off") - the track's
    // readyState stays 'live', only `muted` flips true, and it does not
    // reliably un-mute itself even once the app is foregrounded again.
    // Dropping it here means _ensureStream() below re-requests a fresh
    // stream instead of reusing one that will never produce audio again.
    const alive = this.stream.getTracks().every(
      (track) => track.readyState === 'live' && !track.muted,
    )
    if (!alive) {
      this.stream.getTracks().forEach((track) => track.stop())
      this.stream = null
    }
  }

  async _ensureStream() {
    if (this.stream) return true

    if (typeof navigator === 'undefined' || !navigator.mediaDevices?.getUserMedia) {
      this.error = 'Microphone access is not supported in this browser.'
      this._setPhase('error')
      return false
    }
    if (typeof MediaRecorder === 'undefined') {
      this.error = 'Audio recording is not supported in this browser.'
      this._setPhase('error')
      return false
    }

    try {
      this.stream = await navigator.mediaDevices.getUserMedia({ audio: true })
    } catch {
      this.error = 'Microphone access was denied or is unavailable.'
      this._setPhase('error')
      return false
    }
    this.mimeType = pickSupportedMimeType()
    return true
  }

  _createRecorder() {
    try {
      return this.mimeType
        ? new MediaRecorder(this.stream, { mimeType: this.mimeType })
        : new MediaRecorder(this.stream)
    } catch {
      this.error = 'Recording is not supported in this browser.'
      this._setPhase('error')
      return null
    }
  }

  destroy() {
    if (this._activeRecorder && this._activeRecorder.state !== 'inactive') {
      try {
        this._activeRecorder.stop()
      } catch {
        // ignore
      }
    }
    this._activeRecorder = null

    if (this._activeSourceNode) {
      try {
        this._activeSourceNode.stop()
      } catch {
        // ignore
      }
      this._activeSourceNode = null
    }
    if (this._activeOscillator) {
      try {
        this._activeOscillator.stop()
      } catch {
        // ignore
      }
      this._activeOscillator = null
    }

    if (this.stream) {
      this.stream.getTracks().forEach((track) => track.stop())
      this.stream = null
    }
    if (this._audioCtx) {
      this._audioCtx.close().catch(() => {})
      this._audioCtx = null
    }

    // Reset the real, internal phase (not just whatever mirror a caller
    // might keep) - otherwise a destroy() mid-session (e.g. a host
    // abandoning an in-progress recording) leaves `this.phase` stuck at
    // its last value forever, since nothing else in this class resets it.
    // Every future start()/recordFor() call checks this exact field, so a
    // stale non-idle/non-error value here silently no-ops every call after,
    // with no error and no way to recover short of recreating the engine.
    this.error = null
    this._setPhase('idle')
  }

  async _ensureAudioContext() {
    const AudioContextClass = window.AudioContext || window.webkitAudioContext
    if (!AudioContextClass) return

    // `.state` alone isn't trustworthy - confirmed by a real repro where
    // ctx.state reported 'running' throughout while actually wedged (no
    // onended ever fired). A prior playback/beep timing out is a stronger
    // signal than the state string, so honor it unconditionally here.
    if (this._audioCtxSuspect && this._audioCtx) {
      this._audioCtx.close().catch(() => {})
      this._audioCtx = null
      this._audioCtxSuspect = false
    }

    if (!this._audioCtx) {
      this._audioCtx = new AudioContextClass()
    }
    // WebKit has a non-standard 'interrupted' state (used for backgrounding,
    // Siri, phone calls) on top of the standard 'suspended' - checking only
    // for 'suspended' (as this used to) never even attempts recovery from
    // it. resume() against 'interrupted' is itself reported unreliable on
    // iOS, so if it doesn't actually bring the context back to 'running',
    // fall back to recreating it outright rather than leaving a
    // permanently-dead context in place.
    if (this._audioCtx.state !== 'running') {
      await this._audioCtx.resume().catch(() => {})
    }
    if (this._audioCtx.state !== 'running') {
      this._audioCtx.close().catch(() => {})
      this._audioCtx = new AudioContextClass()
    }
  }

  _setPhase(phase) {
    this.phase = phase
    this._emit()
  }

  _emit() {
    if (!this.onChange) return
    this.onChange({ phase: this.phase, error: this.error })
  }
}
