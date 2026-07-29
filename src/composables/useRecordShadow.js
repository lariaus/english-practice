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

export function useRecordShadow({ allowDoubleRecord = true } = {}) {
  const micState = reactive({ phase: 'idle', error: null })
  const micEngine = new MicRecorderEngine({
    onChange: (snapshot) => Object.assign(micState, snapshot),
  })

  const recordSessionActive = ref(false)
  const isShadowing = ref(false)

  let pendingRecordEnd = null
  let recordWantsDouble = false
  let recordInSecondPass = false

  const shadowGesture = useShiftOrLongPress()

  // Same shift-click/long-press gesture as Shadow, but its own independent
  // instance/state - Record and Shadow are pressed independently, so one's
  // gesture-in-progress shouldn't affect the other's. Only meaningful right
  // before a fresh Record session would start; `allowDoubleRecord` lets a
  // host opt out entirely (e.g. a future context where a double recording
  // wouldn't make sense) - defaults on everywhere else.
  const recordGesture = useShiftOrLongPress()

  function handleRecordPointerDown() {
    if (!allowDoubleRecord) return
    recordGesture.handlePointerDown(() => micState.phase === 'idle' || micState.phase === 'error')
  }
  const handleRecordPointerUp = recordGesture.handlePointerUp
  const handleRecordPointerCancel = recordGesture.handlePointerCancel

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
  // (e.g. pause whatever's playing); onEnd fires once the whole session -
  // both passes, if double mode was requested - has finished (e.g. resume
  // it). Both are optional - the word popup has nothing to pause/resume.
  //
  // `event` is only read on the press that starts a fresh session (shift-
  // click/long-press requests a second pass, same convention as Shadow) -
  // ignored on the click that stops a recording, and never re-checked once
  // pass 2 is already under way.
  function toggleRecording(event, onStart, onEnd) {
    if (micState.phase === 'recording') {
      micEngine.stop()
      return
    }
    if (micState.phase === 'idle' || micState.phase === 'error') {
      recordWantsDouble = allowDoubleRecord && recordGesture.consume(event)
      recordInSecondPass = false
      recordSessionActive.value = true
      pendingRecordEnd = onEnd
      onStart?.()
      micEngine.start()
    }
  }

  // Fires once per recording (pass 1's stop+auto-playback, then again for
  // pass 2's if double mode was requested) - only the *second* time (or the
  // only time, in single mode) does the session actually end and
  // pendingRecordEnd fire. A beep marks the pass-1-finished/pass-2-starting
  // transition; pass 1 itself and the very end of pass 2 stay silent, same
  // as a normal single recording.
  watch(
    () => micState.phase,
    (phase) => {
      if (phase !== 'idle' && phase !== 'error') return
      if (!recordSessionActive.value) return

      if (recordWantsDouble && !recordInSecondPass) {
        recordInSecondPass = true
        // Re-check recordWantsDouble after the beep, not just before it -
        // destroy() (e.g. the popup closing mid-beep) resets it to false,
        // and without this guard the pending .then() would still fire and
        // start a stray recording on an already-torn-down session.
        micEngine.playBeep().then(() => {
          if (recordWantsDouble) micEngine.start()
        })
        return
      }

      recordSessionActive.value = false
      recordWantsDouble = false
      recordInSecondPass = false
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
    recordWantsDouble = false
    recordInSecondPass = false
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
    handleRecordPointerDown,
    handleRecordPointerUp,
    handleRecordPointerCancel,
    destroy,
  }
}
