// One Play/Record/Shadow trio for a single flashcard face (front or back) -
// two independent instances (front, back) are created per session screen
// (FlashcardsLearnScreen.vue/FlashcardsReviewScreen.vue/
// FlashcardsPracticeScreen.vue), same as DictionaryPopup.vue's single
// instance for its one word. `getText()` is called fresh on every Play/
// Shadow press, so it should read whatever the current card's front/back
// is at press time (e.g. `() => currentCard.value.front`).
//
// A flashcard face is often a whole phrase ("voiture rouge"), which has no
// real dictionary audio clip to prefer - only plain TTS makes sense there.
// But when a face is a single word, it's worth preferring the same real
// US pronunciation audio DictionaryPopup.vue uses (via wordAudioPlayer.js's
// playWordPronunciation/playWordPronunciationTimed - dictionary audio
// first, Google TTS fallback), rather than always going straight to TTS.
//
// Speaks/records the text with its IPA (/.../ ) and parenthetical (...)
// annotations stripped out first (see flashcardAnnotationSegmenter.js) -
// "record (noun)" is spoken as just "record". `hasSpeakableText` is false
// whenever that strips down to nothing (e.g. a back side that's pure IPA,
// "/ˈhɪs.t̬ɚ.i/") - the host screen hides the whole Play/Record/Shadow
// trio in that case rather than having it act on an empty phrase.
import { computed, reactive } from 'vue'
import { stripAnnotations } from '../engine/flashcardAnnotationSegmenter.js'
import {
  playTextAloud,
  playTextAloudTimed,
  playWordPronunciation,
  playWordPronunciationTimed,
} from '../engine/wordAudioPlayer.js'
import { useRecordShadow } from './useRecordShadow.js'

// No whitespace left after trimming - a single word/token, not a phrase.
// Exported since useShadowLoop.js's Flashcards integration needs the exact
// same real-audio-vs-TTS dispatch rule for its own speak() callback.
export function isSingleWord(text) {
  return text.length > 0 && !/\s/.test(text.trim())
}

export function useFlashcardFaceAudio(getText) {
  const recordShadow = useRecordShadow()
  const { micState, micEngine, isShadowing, consumeWantsDouble } = recordShadow

  const speakableText = computed(() => stripAnnotations(getText()))
  const hasSpeakableText = computed(() => speakableText.value.length > 0)

  function handlePlayClick() {
    if (isSingleWord(speakableText.value)) {
      playWordPronunciation(speakableText.value)
    } else {
      playTextAloud(speakableText.value)
    }
  }

  // One play-phrase / beep / record / beep / listen-to-yourself pass - same
  // sequence as DictionaryPopup.vue's runWordShadowPass(), preferring real
  // dictionary audio for a single-word face the same way handlePlayClick
  // does. The beep (the same one the double-record transition already
  // uses) marks each transition - into recording, and out of it into
  // playback.
  async function runShadowPass() {
    const elapsedSeconds = isSingleWord(speakableText.value)
      ? await playWordPronunciationTimed(speakableText.value)
      : await playTextAloudTimed(speakableText.value)
    await micEngine.playBeep()
    const blob = await micEngine.recordFor(elapsedSeconds + 0.75)
    await micEngine.playBeep()
    if (blob) await micEngine.playBlob(blob)
  }

  async function handleShadowClick(event) {
    if (micState.phase === 'recording') {
      micEngine.stop()
      return
    }
    if (isShadowing.value) return

    if (micState.phase === 'idle' || micState.phase === 'error') {
      const wantsDouble = consumeWantsDouble(event)
      isShadowing.value = true
      await runShadowPass()
      if (wantsDouble) await runShadowPass()
      isShadowing.value = false
    }
  }

  return reactive({
    hasSpeakableText,
    micState,
    recordSessionActive: recordShadow.recordSessionActive,
    isShadowing,
    recordLabel: recordShadow.recordLabel,
    shadowLabel: recordShadow.shadowLabel,
    toggleRecording: recordShadow.toggleRecording,
    handleShadowPointerDown: recordShadow.handleShadowPointerDown,
    handleShadowPointerUp: recordShadow.handleShadowPointerUp,
    handleShadowPointerCancel: recordShadow.handleShadowPointerCancel,
    handleRecordPointerDown: recordShadow.handleRecordPointerDown,
    handleRecordPointerUp: recordShadow.handleRecordPointerUp,
    handleRecordPointerCancel: recordShadow.handleRecordPointerCancel,
    handlePlayClick,
    handleShadowClick,
  })
}
