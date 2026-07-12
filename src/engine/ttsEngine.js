// Wraps a single TTS backend behind a uniform speak()/stop()/prime()
// interface, so robotShadowingEngine.js doesn't need to know which backend
// is in use. Supported identifier formats:
//   "default-engine:<VoiceName>" - browser's built-in speechSynthesis,
//                                  using the named en-US system voice.
//   "google-tts-api"             - unofficial Google Translate TTS endpoint
//                                  (no API key, but no CORS headers either -
//                                  see _speakGoogleTts for why that matters).

const GOOGLE_TTS_ID = 'google-tts-api'
const DEFAULT_ENGINE_PREFIX = 'default-engine:'

// Options offered in the UI voice picker, in display order.
export const TTS_VOICE_OPTIONS = [
  { id: 'google-tts-api', label: 'Google TTS API' },
  { id: 'default-engine:Samantha', label: 'Default Engine (Samantha)' },
]

// A ~zero-length silent WAV, used purely to "activate" the shared <audio>
// element inside a user gesture so later src changes/play() calls aren't
// blocked as unrequested autoplay.
const SILENT_WAV_DATA_URL =
  'data:audio/wav;base64,UklGRigAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQAAAAA='

function parseIdentifier(identifier) {
  if (identifier === GOOGLE_TTS_ID) return { type: 'google-tts-api' }
  if (identifier?.startsWith(DEFAULT_ENGINE_PREFIX)) {
    return { type: 'default-engine', voiceName: identifier.slice(DEFAULT_ENGINE_PREFIX.length) }
  }
  return { type: 'unknown' }
}

/** @returns {SpeechSynthesisVoice | null} */
function findBrowserVoice(name) {
  if (typeof speechSynthesis === 'undefined') return null
  return speechSynthesis.getVoices().find((v) => v.name === name && v.lang === 'en-US') || null
}

export class TTSEngine {
  /**
   * Pure filter: given candidate identifiers, returns only the ones
   * actually usable on this device/browser, same relative order.
   * @param {string[]} candidates
   * @returns {string[]}
   */
  static getAvailableVoices(candidates) {
    return candidates.filter((id) => {
      const parsed = parseIdentifier(id)
      if (parsed.type === 'google-tts-api') return true // assumed available, not checked
      if (parsed.type === 'default-engine') return !!findBrowserVoice(parsed.voiceName)
      return false
    })
  }

  constructor(identifier) {
    this.identifier = identifier
    this._parsed = parseIdentifier(identifier)

    this._activeUtterance = null
    this._activeResolve = null
    this._audioEl = typeof Audio !== 'undefined' ? new Audio() : null
    this._lastGoogleTtsText = null // for skipping a re-fetch of the same text

    // Google's endpoint 404s cross-origin requests that carry a Referer
    // header (hotlink protection) - browsers send one by default for
    // <audio src>, so this must be suppressed for it to work at all. Some
    // browsers only reliably honor referrerPolicy on elements actually
    // attached to the document, not a detached `new Audio()` - so attach
    // it (hidden) rather than leaving it detached.
    if (this._audioEl) {
      this._audioEl.referrerPolicy = 'no-referrer'
      this._audioEl.setAttribute('referrerpolicy', 'no-referrer')
      this._audioEl.style.display = 'none'
      if (typeof document !== 'undefined' && document.body) {
        document.body.appendChild(this._audioEl)
      }
    }
  }

  // Call synchronously within the Start button's click handler, before any
  // `await` - so later speak() calls aren't blocked as unrequested autoplay.
  prime() {
    if (this._parsed.type === 'default-engine' && typeof speechSynthesis !== 'undefined') {
      try {
        speechSynthesis.cancel()
        const warm = new SpeechSynthesisUtterance(' ')
        warm.volume = 0
        speechSynthesis.speak(warm)
      } catch {
        // Not fatal - real utterances later may just be more likely to be
        // dropped on some browsers if this priming didn't take.
      }
    } else if (this._parsed.type === 'google-tts-api' && this._audioEl) {
      try {
        this._audioEl.src = SILENT_WAV_DATA_URL
        this._audioEl.play().catch(() => {})
      } catch {
        // ignore
      }
    }
  }

  // Speaks `text` aloud. Resolves with the elapsed speaking time in
  // seconds once done (or on error), never rejects.
  speak(text) {
    if (this._parsed.type === 'google-tts-api') return this._speakGoogleTts(text)
    return this._speakDefaultEngine(text)
  }

  stop() {
    if (this._activeUtterance) {
      try {
        speechSynthesis.cancel()
      } catch {
        // ignore
      }
      this._activeUtterance = null
    }
    if (this._audioEl) {
      try {
        this._audioEl.pause()
      } catch {
        // ignore
      }
    }
    const resolve = this._activeResolve
    this._activeResolve = null
    if (resolve) resolve(1)
  }

  _speakDefaultEngine(text) {
    return new Promise((resolve) => {
      const voice = this._parsed.voiceName ? findBrowserVoice(this._parsed.voiceName) : null
      const utterance = new SpeechSynthesisUtterance(text)
      if (voice) utterance.voice = voice

      this._activeUtterance = utterance
      this._activeResolve = resolve

      let startedAt = Date.now()
      utterance.onstart = () => {
        startedAt = Date.now()
      }

      const finish = () => {
        this._activeUtterance = null
        const elapsedSeconds = Math.max(1, (Date.now() - startedAt) / 1000)
        const resolveFn = this._activeResolve
        this._activeResolve = null
        if (resolveFn) resolveFn(elapsedSeconds)
      }
      utterance.onend = finish
      utterance.onerror = finish

      speechSynthesis.speak(utterance)
    })
  }

  // Unofficial endpoint, no CORS headers - can't be read via fetch() +
  // decodeAudioData(), so this plays through a plain <audio> element
  // instead (media elements can load/play cross-origin without CORS).
  // Reuses one persistent element (primed above) rather than creating a
  // new Audio() per phrase, since a fresh element deep in the async loop
  // is exactly what got silently autoplay-blocked once before in this app
  // (see recorderLoopEngine's playback fix).
  _speakGoogleTts(text) {
    return new Promise((resolve) => {
      if (!this._audioEl) {
        resolve(1)
        return
      }

      this._activeResolve = resolve
      const startedAt = Date.now()

      const finish = () => {
        this._audioEl.removeEventListener('ended', finish)
        this._audioEl.removeEventListener('error', finish)
        const elapsedSeconds = Math.max(1, (Date.now() - startedAt) / 1000)
        const resolveFn = this._activeResolve
        this._activeResolve = null
        if (resolveFn) resolveFn(elapsedSeconds)
      }
      this._audioEl.addEventListener('ended', finish)
      this._audioEl.addEventListener('error', finish)

      // Same phrase as the last request (e.g. a shadowing repeat, or the
      // "repeat model" replay) - just replay what's already loaded instead
      // of hitting the network again.
      if (this._lastGoogleTtsText === text) {
        this._audioEl.currentTime = 0
        this._audioEl.play().catch(finish)
        return
      }

      this._lastGoogleTtsText = text
      const url = `https://translate.google.com/translate_tts?ie=UTF-8&q=${encodeURIComponent(text)}&tl=en-US&client=tw-ob`
      this._audioEl.src = url
      this._audioEl.play().catch(finish)
    })
  }
}
