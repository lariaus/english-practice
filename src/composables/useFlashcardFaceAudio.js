// One Play/Record/Shadow trio for a single flashcard face (front or back) -
// two independent instances (front, back) are created per session screen
// (FlashcardsLearnScreen.vue/FlashcardsReviewScreen.vue/
// FlashcardsPracticeScreen.vue), same as DictionaryPopup.vue's single
// instance for its one word. `getText()` is called fresh on every Play/
// Shadow press, so it should read whatever the current card's front/back
// is at press time (e.g. `() => currentCard.value.front`).
//
// Deliberately plain TTS via wordAudioPlayer.js's playTextAloud/
// playTextAloudTimed, not playWordPronunciation - unlike a single
// dictionary word, a flashcard face is often a whole phrase, so there's no
// real dictionary audio clip to prefer in the first place.
//
// Speaks/records the text with its IPA (/.../ ) and parenthetical (...)
// annotations stripped out first (see flashcardAnnotationSegmenter.js) -
// "record (noun)" is spoken as just "record". `hasSpeakableText` is false
// whenever that strips down to nothing (e.g. a back side that's pure IPA,
// "/ˈhɪs.t̬ɚ.i/") - the host screen hides the whole Play/Record/Shadow
// trio in that case rather than having it act on an empty phrase.
import { computed, reactive } from 'vue'
import { stripAnnotations } from '../engine/flashcardAnnotationSegmenter.js'
import { playTextAloud, playTextAloudTimed } from '../engine/wordAudioPlayer.js'
import { useRecordShadow } from './useRecordShadow.js'

export function useFlashcardFaceAudio(getText) {
  const recordShadow = useRecordShadow()
  const { micState, micEngine, isShadowing, consumeWantsDouble } = recordShadow

  const speakableText = computed(() => stripAnnotations(getText()))
  const hasSpeakableText = computed(() => speakableText.value.length > 0)

  function handlePlayClick() {
    playTextAloud(speakableText.value)
  }

  // One play-phrase / record / listen-to-yourself pass - same sequence as
  // DictionaryPopup.vue's runWordShadowPass(), just via plain TTS instead
  // of a real dictionary audio clip.
  async function runShadowPass() {
    const elapsedSeconds = await playTextAloudTimed(speakableText.value)
    const blob = await micEngine.recordFor(elapsedSeconds + 0.25)
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
