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
    this._activeRecorder = null
    this._activeSourceNode = null
    this._activeOscillator = null
    this._lastBlob = null

    this._emit()
  }

  async start() {
    if (this.phase !== 'idle' && this.phase !== 'error') return

    this.error = null
    this._ensureAudioContext()
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
    this._ensureAudioContext()
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

  async playBlob(blob) {
    if (!blob) {
      this._setPhase('idle')
      return
    }

    this._setPhase('playing')

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
      source.onended = () => {
        this._activeSourceNode = null
        resolve()
      }
      source.start()
    })

    this._setPhase('idle')
  }

  playBeep() {
    return new Promise((resolve) => {
      this._ensureAudioContext()
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
      oscillator.onended = () => {
        this._activeOscillator = null
        resolve()
      }

      oscillator.start(now)
      oscillator.stop(now + BEEP_DURATION_MS / 1000)
    })
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

  _setPhase(phase) {
    this.phase = phase
    this._emit()
  }

  _emit() {
    if (!this.onChange) return
    this.onChange({ phase: this.phase, error: this.error })
  }
}
