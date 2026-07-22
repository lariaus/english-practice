// Plays a word's pronunciation - the shared, reusable "make a word audible"
// primitive, callable from anywhere in the app (not just WordInfoPopup).
//
// Structurally different from ttsEngine.js's TTSEngine: that class commits
// to a single user-selected voice backend for speaking arbitrary text (pick
// "Google TTS" or "Samantha" once, use it for a whole session). Here we
// instead try multiple heterogeneous pronunciation *sources* in priority
// order for one specific word - real dictionary audio first, synthetic TTS
// only as a fallback - so it's plain functions trying a fallback chain, not
// a single selectable backend.
//
// Priority order:
//   1. The US pronunciation from dictionaryClient.js's fetchWordInfo(), if
//      that word's dictionary entry has one.
//   2. Google Translate TTS, via ttsEngine.js's TTSEngine (reused rather
//      than reimplemented) - covers everything else, e.g. a word whose
//      dictionary entry only has UK/AU audio and no US recording.

import { fetchWordInfo } from './dictionaryClient.js'
import { TTSEngine } from './ttsEngine.js'

export function playAudioUrl(url, rate = 1) {
  if (!url) return
  const audio = new Audio(url)
  audio.playbackRate = rate
  audio.play().catch(() => {})
}

// Same playback, but awaitable - resolves with the elapsed seconds once
// playback actually finishes (or on error), never rejects. Used by Shadow's
// record-time calculation, which needs to know how long the clip took.
export function playAudioUrlTimed(url) {
  return new Promise((resolve) => {
    if (!url) {
      resolve(0)
      return
    }
    const audio = new Audio(url)
    const startedAt = Date.now()
    const finish = () => resolve(Math.max(0.5, (Date.now() - startedAt) / 1000))
    audio.addEventListener('ended', finish)
    audio.addEventListener('error', finish)
    audio.play().catch(finish)
  })
}

// Lazily created, reused across calls - TTSEngine's constructor appends a
// hidden <audio> element to the page, so a fresh instance per call would
// leak one into the DOM every time.
let googleTts = null
function getGoogleTts() {
  if (!googleTts) googleTts = new TTSEngine('google-tts-api')
  return googleTts
}

// Plays the best available pronunciation for `word`: a real US dictionary
// recording if one exists, otherwise Google Translate TTS. `rate` (1 =
// normal) is used by the popup's shift-click/long-press "play slower"
// gesture - see WordInfoPopup.vue.
export async function playWordPronunciation(word, lang = 'en', rate = 1) {
  const info = await fetchWordInfo(word, lang)
  const usEntry = info?.phonetics.find((p) => p.label === 'US' && p.audio)

  if (usEntry) {
    playAudioUrl(usEntry.audio, rate)
    return
  }

  getGoogleTts().speak(word, rate)
}

// Same fallback chain as playWordPronunciation, but awaitable - resolves
// with the elapsed seconds once the clip actually finishes playing. Used by
// Shadow (see useRecordShadow.js), which needs to know how long the word
// took to say before sizing its own recording window.
export async function playWordPronunciationTimed(word, lang = 'en') {
  const info = await fetchWordInfo(word, lang)
  const usEntry = info?.phonetics.find((p) => p.label === 'US' && p.audio)

  if (usEntry) return playAudioUrlTimed(usEntry.audio)

  return getGoogleTts().speak(word)
}
