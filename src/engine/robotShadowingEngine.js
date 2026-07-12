// Framework-agnostic engine for the Robot Shadowing tool.
//
// Owns the beep -> speak (TTS) -> beep -> record -> beep -> playback -> ...
// state machine, mic/stream lifecycle, MediaRecorder setup, and Wake Lock.
// Mirrors recorderLoopEngine.js's patterns (beep-cued phase transitions,
// AudioContext-based playback, stop()-safe interruption of whatever's
// active) with a TTS "speaking" phase and random phrase selection added.

import dbEasy from '../data/db-shadowing-easy.json'
import dbMedium from '../data/db-shadowing-medium.json'
import dbHard from '../data/db-shadowing-hard.json'
import { TTSEngine } from './ttsEngine.js'

const COUNTDOWN_TICK_MS = 200
const BEEP_DURATION_MS = 150
const BEEP_FREQUENCY_HZ = 880

const MIME_TYPE_CANDIDATES = [
  'audio/mp4', // Safari (iOS/macOS)
  'audio/webm;codecs=opus',
  'audio/webm',
  'audio/ogg;codecs=opus',
]

// Options offered in the UI database picker, in display order.
export const PHRASE_DATABASE_OPTIONS = [
  { id: 'easy', label: 'Easy', phrases: dbEasy },
  { id: 'medium', label: 'Medium', phrases: dbMedium },
  { id: 'hard', label: 'Hard', phrases: dbHard },
]

function getDatabase(databaseId) {
  return (
    PHRASE_DATABASE_OPTIONS.find((option) => option.id === databaseId)?.phrases || dbEasy
  )
}

function pickSupportedMimeType() {
  if (typeof MediaRecorder === 'undefined' || !MediaRecorder.isTypeSupported) {
    return ''
  }
  return MIME_TYPE_CANDIDATES.find((type) => MediaRecorder.isTypeSupported(type)) || ''
}

function pickRandomPhrase(phrases, exclude) {
  if (phrases.length <= 1) return phrases[0]
  let phrase
  do {
    phrase = phrases[Math.floor(Math.random() * phrases.length)]
  } while (phrase === exclude)
  return phrase
}

export class RobotShadowingEngine {
  constructor({ onChange } = {}) {
    // recordSeconds isn't fixed - it's set after each phrase is spoken, to
    // (actual TTS speaking time) + 1s, so the response window matches how
    // long the phrase itself took to say.
    this.recordSeconds = 0
    this._lastSpeakSeconds = 2 // fallback, always overwritten after the first _speakPhase
    this.repeatCount = 1
    this.repeatModel = false
    this._phrases = dbEasy
    this.onChange = onChange

    this.phase = 'idle' // idle | requesting-permission | speaking | recording | playing | error
    this.secondsRemaining = 0
    this.error = null

    this.stream = null
    this.mimeType = ''
    this._stopRequested = false
    this._lastBlob = null
    this._lastPhrase = null

    this._audioCtx = null
    this._activeRecorder = null
    this._activeOscillator = null
    this._activeSourceNode = null
    this._activeResolve = null
    this._tickInterval = null
    this._wakeLock = null
    this._tts = null

    this._onVisibilityChange = () => {
      if (document.visibilityState === 'visible' && this.stream && !this._stopRequested) {
        this._acquireWakeLock()
      }
    }

    this._emit()
  }

  async start(ttsIdentifier, repeatCount = 1, repeatModel = false, databaseId = 'easy') {
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
    const availableIds = TTSEngine.getAvailableVoices([ttsIdentifier])
    if (availableIds.length === 0) {
      this.error = 'That text-to-speech voice is not available in this browser.'
      this._setPhase('error')
      return
    }

    this.error = null
    this._stopRequested = false
    this.repeatCount = repeatCount
    this.repeatModel = repeatModel
    this._phrases = getDatabase(databaseId)
    this._setPhase('requesting-permission')

    this._tts = new TTSEngine(ttsIdentifier)

    // Same "unlock within the click gesture" trick used for the AudioContext
    // beeps: prime it synchronously here, before any `await`, so later
    // speak() calls aren't silently blocked as unrequested autoplay.
    this._ensureAudioContext()
    this._tts.prime()

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
    if (this._tts) {
      this._tts.stop()
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
      const phrase = pickRandomPhrase(this._phrases, this._lastPhrase)
      this._lastPhrase = phrase

      for (let i = 0; i < this.repeatCount && !this._stopRequested; i++) {
        await this._speakPhase(phrase)
        if (this._stopRequested) break
        await this._recordPhase()
        if (this._stopRequested) break
        if (this.repeatModel) {
          await this._speakPhase(phrase)
          if (this._stopRequested) break
        }
        await this._playPhase()
      }
    }
  }

  async _speakPhase(phrase) {
    this.secondsRemaining = 0
    this._setPhase('speaking')

    await this._playBeep()
    if (this._stopRequested) return

    this._lastSpeakSeconds = await this._tts.speak(phrase)
  }

  async _recordPhase() {
    this.recordSeconds = this._lastSpeakSeconds + 1
    this.secondsRemaining = this.recordSeconds
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
      this._startCountdown(this.recordSeconds, () => {
        if (recorder.state !== 'inactive') recorder.stop()
      })
    })
  }

  async _playPhase() {
    if (!this._lastBlob) return

    this.secondsRemaining = this.recordSeconds
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
      this._startCountdown(this.recordSeconds, () => {})
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

  _startCountdown(durationSeconds, onComplete) {
    this._stopCountdown()
    const durationMs = durationSeconds * 1000
    const startedAt = Date.now()
    this.secondsRemaining = durationSeconds
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
      totalSeconds: this.recordSeconds,
      error: this.error,
    })
  }
}
