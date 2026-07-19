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

export function playAudioUrl(url) {
  if (!url) return
  new Audio(url).play().catch(() => {})
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
// recording if one exists, otherwise Google Translate TTS.
export async function playWordPronunciation(word, lang = 'en') {
  const info = await fetchWordInfo(word, lang)
  const usEntry = info?.phonetics.find((p) => p.label === 'US' && p.audio)

  if (usEntry) {
    playAudioUrl(usEntry.audio)
    return
  }

  getGoogleTts().speak(word)
}
