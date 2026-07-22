// Shared plumbing behind every Record/Shadow button pair in the app: owns
// the mic engine, the Record toggle (start/stop, auto-plays back on stop),
// Shadow's shift-click/long-press "run it twice" gesture detection, and the
// R/S/L and S/R/L/P button labels - the parts that are genuinely identical
// everywhere a Record+Shadow pair shows up (YT Shadowing's own controls, the
// word popup, and presumably more later).
//
// What a single Shadow *pass* actually does - what "the original" is, and
// in what order it plays/records/listens - differs by context (e.g. YT
// Shadowing's video segment, where the recording's real duration decides
// what gets replayed afterward, vs a word's fixed-length pronunciation clip,
// played up front so its duration is known before recording starts). That
// sequence stays owned by each call site, built from the pieces exposed
// here (micEngine, micState, isShadowing, consumeWantsDouble) rather than
// forced into one shape that would fit neither case well.
import { computed, onBeforeUnmount, reactive, ref, watch } from 'vue'
import { MicRecorderEngine } from '../engine/micRecorderEngine.js'
import { useShiftOrLongPress } from './useShiftOrLongPress.js'

export function useRecordShadow() {
  const micState = reactive({ phase: 'idle', error: null })
  const micEngine = new MicRecorderEngine({
    onChange: (snapshot) => Object.assign(micState, snapshot),
  })

  const recordSessionActive = ref(false)
  const isShadowing = ref(false)

  let pendingRecordEnd = null

  const shadowGesture = useShiftOrLongPress()

  // Only meaningful right before a fresh Shadow session would start -
  // holding the button down elsewhere (mid-recording, mid-playback) has no
  // effect, same as a stray shiftKey would have no effect there.
  function handleShadowPointerDown() {
    shadowGesture.handlePointerDown(
      () => !isShadowing.value && (micState.phase === 'idle' || micState.phase === 'error'),
    )
  }
  const handleShadowPointerUp = shadowGesture.handlePointerUp
  const handleShadowPointerCancel = shadowGesture.handlePointerCancel

  // Reads (and resets) whether the press that's starting a fresh Shadow
  // session requested the double pass - shift-click, Shift+key, or a
  // long-press. Only meaningful at that first press; callers shouldn't
  // check this again for later presses in the same session.
  const consumeWantsDouble = shadowGesture.consume

  // onStart fires synchronously when a fresh recording actually begins
  // (e.g. pause whatever's playing); onEnd fires once that recording and
  // its automatic playback have both finished (e.g. resume it). Both are
  // optional - the word popup has nothing to pause/resume.
  function toggleRecording(onStart, onEnd) {
    if (micState.phase === 'recording') {
      micEngine.stop()
      return
    }
    if (micState.phase === 'idle' || micState.phase === 'error') {
      recordSessionActive.value = true
      pendingRecordEnd = onEnd
      onStart?.()
      micEngine.start()
    }
  }

  watch(
    () => micState.phase,
    (phase) => {
      if (phase !== 'idle' && phase !== 'error') return
      if (!recordSessionActive.value) return
      recordSessionActive.value = false
      const cb = pendingRecordEnd
      pendingRecordEnd = null
      cb?.()
    },
  )

  const recordLabel = computed(() => {
    if (!recordSessionActive.value) return 'R'
    switch (micState.phase) {
      case 'requesting-permission':
        return 'Requesting mic…'
      case 'recording':
        return 'S'
      case 'playing':
        return 'L'
      default:
        return 'R'
    }
  })

  // S (idle) - R (recording) - L (listening to your own playback) - P
  // (hearing/rehearing the original) - same codes regardless of what a
  // pass's sequence actually looks like, since they only reflect mic phase.
  const shadowLabel = computed(() => {
    if (!isShadowing.value) return 'S'
    switch (micState.phase) {
      case 'recording':
        return 'R'
      case 'playing':
        return 'L'
      default:
        return 'P'
    }
  })

  // Fully stops and resets everything - micEngine.destroy() now resets its
  // own phase/error and emits, so micState follows automatically; this just
  // also clears the two flags that live here, so a host can call this to
  // abandon an in-progress Record/Shadow session (e.g. the word popup
  // closing mid-recording) and have the UI immediately reflect a clean idle
  // state, not a stale "recording"/"listening" one.
  function destroy() {
    micEngine.destroy()
    recordSessionActive.value = false
    isShadowing.value = false
  }

  onBeforeUnmount(destroy)

  return {
    micState,
    micEngine,
    recordSessionActive,
    isShadowing,
    recordLabel,
    shadowLabel,
    toggleRecording,
    consumeWantsDouble,
    handleShadowPointerDown,
    handleShadowPointerUp,
    handleShadowPointerCancel,
    destroy,
  }
}
