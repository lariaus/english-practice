// Generic hands-free shuffle-and-shadow loop: given a caller-supplied list
// of opaque items and a `speak(item)` callback that plays that item's audio
// and resolves with how many seconds it took, repeatedly picks a random
// item (excluding only the immediately-previous one - no broader no-repeat
// window, matching robotShadowingEngine.js's own pickRandomPhrase exactly)
// and runs a beep -> speak -> beep -> record -> [beep -> re-speak, only if
// repeatModel] -> beep -> playback cycle, `repeatCount` times per item
// before picking a new one. See docs/shadow-loop-mode-spec.md.
//
// Deliberately built on MicRecorderEngine's existing playBeep/recordFor/
// playBlob/destroy primitives (same as useFlashcardFaceAudio.js's own
// runShadowPass) rather than reimplementing MediaRecorder/beep machinery
// the way robotShadowingEngine.js does independently - this is the one
// piece of real logic worth sharing, so a future caller (e.g. a YT
// Shadowing transcript-word loop) gets it for free just by supplying its
// own item list and speak() callback, with no knowledge of MediaRecorder/
// AudioContext at all.
//
// `speak` deciding HOW to play an item (real dictionary audio vs TTS,
// single word vs phrase) is intentionally the caller's job, not this
// composable's - it has no opinion on audio sourcing, only on sequencing.
//
// The +0.75s recording-window bump (not Robot Shadowing's own +1s) is
// baked in here rather than left to the caller, since every current use of
// this composable wants Flashcards' Shadow-button timing specifically.
import { onBeforeUnmount, reactive, ref } from 'vue'
import { MicRecorderEngine } from '../engine/micRecorderEngine.js'

const RECORD_WINDOW_PADDING_SECONDS = 0.75

function pickRandom(items, exclude) {
  if (items.length <= 1) return items[0]
  let item
  do {
    item = items[Math.floor(Math.random() * items.length)]
  } while (item === exclude)
  return item
}

export function useShadowLoop({ speak }) {
  const micState = reactive({ phase: 'idle', error: null })
  const micEngine = new MicRecorderEngine({
    onChange: (snapshot) => Object.assign(micState, snapshot),
  })

  const isActive = ref(false)
  const currentItem = ref(null)
  // idle | speaking | recording | replaying-model | playing-back
  const loopPhase = ref('idle')

  let stopRequested = false
  let lastItem = null

  async function start(items, { repeatCount = 1, repeatModel = false } = {}) {
    if (isActive.value || !items || items.length === 0) return

    stopRequested = false
    isActive.value = true

    while (!stopRequested) {
      const item = pickRandom(items, lastItem)
      lastItem = item
      currentItem.value = item

      for (let i = 0; i < repeatCount && !stopRequested; i++) {
        loopPhase.value = 'speaking'
        await micEngine.playBeep()
        if (stopRequested) break
        const elapsedSeconds = await speak(item)
        if (stopRequested) break

        loopPhase.value = 'recording'
        await micEngine.playBeep()
        if (stopRequested) break
        const blob = await micEngine.recordFor(elapsedSeconds + RECORD_WINDOW_PADDING_SECONDS)
        if (stopRequested) break

        if (repeatModel) {
          loopPhase.value = 'replaying-model'
          await micEngine.playBeep()
          if (stopRequested) break
          await speak(item)
          if (stopRequested) break

          // No beep here, deliberately - unlike every other transition,
          // this one goes straight from playing the model audio again into
          // playing back your recording, both passive listening with
          // nothing for you to *do* in between - a beep only marks a real
          // active/passive transition (about to speak, about to record),
          // which this isn't.
          loopPhase.value = 'playing-back'
          if (blob) await micEngine.playBlob(blob)
        } else {
          loopPhase.value = 'playing-back'
          await micEngine.playBeep()
          if (stopRequested) break
          if (blob) await micEngine.playBlob(blob)
        }
      }
    }

    isActive.value = false
    loopPhase.value = 'idle'
    currentItem.value = null
  }

  // destroy() (not the narrower stop()) so a Stop press cuts off whatever's
  // actively playing right now - a beep, the word/phrase audio, or
  // playback - not just an in-progress recording. destroy() unconditionally
  // tears down and resolves whichever of those is active (mirroring
  // robotShadowingEngine.js's own thorough manual stop()), so the pending
  // await inside start()'s loop above unblocks immediately and the
  // stopRequested check right after it exits cleanly. A later start() call
  // transparently re-acquires the mic stream/AudioContext, same as any
  // other post-destroy() use of this engine elsewhere in the app.
  function stop() {
    stopRequested = true
    micEngine.destroy()
  }

  onBeforeUnmount(stop)

  return reactive({ isActive, currentItem, loopPhase, micState, start, stop })
}
