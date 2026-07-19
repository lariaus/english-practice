<template>
  <main class="screen yt-player-screen">
    <button class="back-button" @click="handleBack">&larr; Back</button>

    <h1>YT Shadowing</h1>

    <p v-if="state.error" class="error-message">{{ state.error }}</p>

    <div class="player-and-transcript">
      <div class="video-column">
        <div class="yt-player-wrap">
          <div id="yt-shadowing-player"></div>

          <div class="capture-overlay" v-if="captureRange">
            <p class="capture-overlay-text">
              Validate capture of {{ formatTime(captureRange.start) }} -
              {{ formatTime(captureRange.end) }}?
            </p>
            <div class="capture-overlay-buttons">
              <button class="overlay-button overlay-button-yes" @click="handleValidateCapture">Yes</button>
              <button class="overlay-button overlay-button-no" @click="handleDiscardCapture">No</button>
            </div>
          </div>

          <div class="subtitle-overlay" v-if="currentCaptionText">
            <span class="subtitle-text">{{ currentCaptionText }}</span>
          </div>
        </div>

        <div class="transport-controls">
          <button class="transport-button" :disabled="controlsDisabled" @click="engine.seekBy(-SEEK_STEP_SECONDS)">
            -{{ SEEK_STEP_SECONDS }}s
          </button>
          <button
            class="transport-button transport-button-main"
            :disabled="controlsDisabled"
            @click="engine.togglePlayPause()"
          >
            {{ state.isPlaying ? 'Pause' : 'Play' }}
          </button>
          <button class="transport-button" :disabled="controlsDisabled" @click="engine.seekBy(SEEK_STEP_SECONDS)">
            +{{ SEEK_STEP_SECONDS }}s
          </button>
          <button
            class="transport-button cc-button"
            :class="{ active: subtitlesEnabled }"
            :disabled="controlsDisabled"
            @click="toggleSubtitles"
          >
            {{ subtitlesLoading ? 'CC…' : 'CC' }}
          </button>
        </div>

        <button
          class="capture-button"
          :class="{ recording: isCapturing, looping: isLoopingMode }"
          :disabled="!isReady || !!captureRange || isShadowing"
          @pointerdown="handleCaptureStart"
          @pointerup="handleCaptureEnd"
          @pointercancel="handleCaptureCancel"
          @click="handleCaptureClick"
        >
          {{ captureButtonLabel }}
        </button>

        <div class="speed-control">
          <button
            class="speed-step-button"
            :disabled="controlsDisabled"
            @click="engine.changeSpeed(-PLAYBACK_RATE_STEP)"
          >
            &minus;
          </button>

          <div class="speed-bar">
            <div class="speed-bar-center-tick"></div>
            <div class="speed-bar-marker" :style="{ left: speedMarkerPercent + '%' }"></div>
          </div>

          <button
            class="speed-step-button"
            :disabled="controlsDisabled"
            @click="engine.changeSpeed(PLAYBACK_RATE_STEP)"
          >
            +
          </button>

          <span class="speed-value">{{ state.playbackRate.toFixed(2) }}x</span>
        </div>

        <div class="mic-controls">
          <button
            class="record-button"
            :class="{ recording: micState.phase === 'recording' }"
            :disabled="controlsDisabled"
            @click="toggleRecording"
          >
            <span class="record-icon">
              <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                <circle cx="12" cy="12" r="7" fill="currentColor" />
              </svg>
            </span>
            <span>{{ recordButtonLabel }}</span>
          </button>

          <button
            class="shadow-button"
            :class="{ active: isShadowing }"
            :disabled="shadowButtonDisabled"
            @click="handleShadow"
          >
            {{ shadowButtonLabel }}
          </button>
        </div>

        <p v-if="micState.error" class="error-message">{{ micState.error }}</p>
      </div>

      <div class="subtitle-panel" v-if="subtitlesEnabled && subtitleCues.length > 0">
        <ul class="subtitle-list" ref="subtitleListEl">
          <li
            v-for="(cue, index) in subtitleCues"
            :key="index"
            class="subtitle-list-item"
            :class="{ active: index === activeCueIndex }"
          >
            <span
              class="subtitle-list-icon"
              :class="{ 'is-active': index === activeCueIndex, disabled: subtitleSeekDisabled }"
              @click="handleSubtitleLineClick(cue, index, $event)"
              >▶</span
            >
            <span class="subtitle-list-text">
              <template v-for="(token, tokenIndex) in splitIntoWords(cue.text)" :key="tokenIndex">
                <span
                  v-if="isClickableWord(token)"
                  class="subtitle-word"
                  @click="handleWordClick(token)"
                  >{{ token }}</span
                ><template v-else>{{ token }}</template>
              </template>
            </span>
          </li>
        </ul>

        <button
          class="capture-selection-button"
          :disabled="captureFromSelectionDisabled"
          @mousedown.prevent
          @click="handleCaptureFromSelection"
        >
          Capture selection
        </button>
      </div>
    </div>

    <WordInfoPopup ref="wordInfoPopup" />
  </main>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue'
import {
  MAX_PLAYBACK_RATE,
  MIN_PLAYBACK_RATE,
  PLAYBACK_RATE_STEP,
  SEEK_STEP_SECONDS,
  YtShadowingEngine,
} from '../engine/ytShadowingEngine.js'
import { addToHistory } from '../engine/ytHistory.js'
import { MicRecorderEngine } from '../engine/micRecorderEngine.js'
import { fetchSubtitles, isNativeExpServerAvailable } from '../engine/nativeExpServerClient.js'
import WordInfoPopup from '../components/WordInfoPopup.vue'

const props = defineProps({
  videoId: { type: String, required: true },
  url: { type: String, required: true },
})

const emit = defineEmits(['back'])

const state = reactive({
  phase: 'idle',
  error: null,
  isPlaying: false,
  playbackRate: 1,
  videoTitle: null,
})

const engine = new YtShadowingEngine({
  onChange: (snapshot) => Object.assign(state, snapshot),
})

const isReady = computed(() => state.phase === 'ready')

const micState = reactive({ phase: 'idle', error: null })
const micEngine = new MicRecorderEngine({
  onChange: (snapshot) => Object.assign(micState, snapshot),
})

const SUBTITLE_POLL_MS = 100

// Best-effort only: stays empty (and the CC button just does nothing
// visible) if NativeExpServer isn't running - see nativeExpServerClient.js.
// Only fetched lazily on the first "CC" click, then cached in subtitleCues
// for the rest of the session - toggling off/on again never re-fetches.
const subtitleCues = ref([])
const subtitlesEnabled = ref(false)
const subtitlesLoading = ref(false)
let subtitlesFetchAttempted = false
const currentTimeSeconds = ref(0)
let subtitlePollInterval = null

async function toggleSubtitles() {
  if (subtitlesEnabled.value) {
    subtitlesEnabled.value = false
    return
  }

  subtitlesEnabled.value = true
  if (subtitlesFetchAttempted) return

  subtitlesFetchAttempted = true
  subtitlesLoading.value = true
  await loadSubtitlesIfAvailable()
  subtitlesLoading.value = false
}

// Auto-generated caption cues routinely overlap (a cue's `duration` is how
// long it stays on screen, not how long the speech takes) - so we always
// want the most recently *started* cue, not just any cue whose window still
// technically contains the current time, or the previous line lingers past
// when the next one should have taken over.
const activeCueIndex = computed(() => {
  if (!subtitlesEnabled.value) return -1
  const t = currentTimeSeconds.value
  const cues = subtitleCues.value
  let index = -1
  for (let i = 0; i < cues.length; i++) {
    if (cues[i].start > t) break
    index = i
  }
  if (index === -1) return -1
  const cue = cues[index]
  return t < cue.start + cue.duration ? index : -1
})

const currentCaptionText = computed(() => {
  const index = activeCueIndex.value
  return index >= 0 ? subtitleCues.value[index].text : ''
})

const subtitleListEl = ref(null)

// Keeps the transcript sidebar scrolled to the currently-playing line, but
// lazily - like Language Reactor's sidebar: don't move anything as long as
// the active line is still comfortably visible, only scroll once it's
// about to disappear off the bottom (or is already off-screen, e.g. after
// seeking), and land it near the top rather than dead-center so upcoming
// lines have room to scroll into view naturally as playback continues.
watch(activeCueIndex, (index) => {
  if (index < 0 || !subtitleListEl.value) return
  const container = subtitleListEl.value
  const item = container.children[index]
  if (!item) return

  const containerRect = container.getBoundingClientRect()
  const itemRect = item.getBoundingClientRect()

  const isAboveView = itemRect.top < containerRect.top
  const isNearOrPastBottom = itemRect.bottom > containerRect.bottom - itemRect.height

  if (isAboveView || isNearOrPastBottom) {
    container.scrollTo({
      top: Math.max(0, item.offsetTop - container.offsetTop),
      behavior: 'smooth',
    })
  }
})

// Auto-generated caption cues routinely overlap (a cue's `duration` is how
// long it stays on screen, not the actual speech length) - so a cue's own
// reported end can extend past where the *next* cue actually starts
// speaking. Clamp to that next cue's start (if it's earlier) so a capture
// scoped to this cue doesn't bleed into the following line.
function getCueRange(index) {
  const cues = subtitleCues.value
  const cue = cues[index]
  if (!cue) return null
  const rawEnd = cue.start + cue.duration
  const nextCueStart = cues[index + 1]?.start
  const end = nextCueStart !== undefined ? Math.min(rawEnd, nextCueStart) : rawEnd
  return { start: cue.start, end }
}

// Shared by both ways of jumping straight into capture/loop mode without
// the Yes/No review overlay (a selection, or shift-clicking a single line) -
// both represent a choice the user already deliberately made.
function enterCaptureLoop(range) {
  engine.pause()
  loopRange.value = range
  isLoopingMode.value = true
  engine.startLoop(range.start, range.end)
}

// A plain click seeks to and plays from this line, same as always. Shift-
// click instead jumps straight into capturing just this one line, skipping
// the Yes/No review - a quicker path than press-and-hold or text-selection
// for "I just want to loop this exact line."
function handleSubtitleLineClick(cue, index, event) {
  if (subtitleSeekDisabled.value) return

  if (event?.shiftKey) {
    const range = getCueRange(index)
    if (!range) return
    enterCaptureLoop(range)
    return
  }

  engine.seekTo(cue.start)
  engine.play()
}

const hasTranscriptSelection = ref(false)

function handleSelectionChange() {
  const selection = document.getSelection()
  hasTranscriptSelection.value = !!(
    selection &&
    !selection.isCollapsed &&
    subtitleListEl.value?.contains(selection.getRangeAt(0).commonAncestorContainer)
  )
}

// Walks up from a selection endpoint (a text node or element) to the
// <li> it belongs to, returning that cue's index - or -1 if it's not
// inside the transcript list at all (e.g. the selection was dragged out
// over a button).
function findCueIndexForNode(node) {
  const el = node.nodeType === Node.TEXT_NODE ? node.parentElement : node
  const li = el?.closest?.('.subtitle-list-item')
  if (!li || !subtitleListEl.value) return -1
  return Array.from(subtitleListEl.value.children).indexOf(li)
}

// "Smallest span of cues that contains the whole selection" - not
// word-precise (cue data only has one timestamp per line, see the capture
// discussion), just rounds outward to the full start/end of whichever
// cues the selection touches.
function getSelectedCueRange() {
  const selection = document.getSelection()
  if (!selection || selection.isCollapsed) return null

  const range = selection.getRangeAt(0)
  const startIndex = findCueIndexForNode(range.startContainer)
  const endIndex = findCueIndexForNode(range.endContainer)
  if (startIndex === -1 || endIndex === -1) return null

  const lo = Math.min(startIndex, endIndex)
  const hi = Math.max(startIndex, endIndex)
  return { start: subtitleCues.value[lo].start, end: getCueRange(hi).end }
}

const captureFromSelectionDisabled = computed(
  () =>
    !isReady.value ||
    isCapturing.value ||
    !!captureRange.value ||
    isShadowing.value ||
    isLoopingMode.value ||
    !hasTranscriptSelection.value,
)

// Unlike the press-and-hold Capture flow, a selection has already been
// deliberately made and confirmed by clicking this button - so it skips
// the Yes/No review overlay entirely and goes straight into capture/loop
// mode, doing exactly what handleValidateCapture() does.
function handleCaptureFromSelection() {
  if (captureFromSelectionDisabled.value) return
  const range = getSelectedCueRange()
  if (!range) return

  document.getSelection()?.removeAllRanges()
  hasTranscriptSelection.value = false
  enterCaptureLoop(range)
}

const wordInfoPopup = ref(null)

// Splits a cue's text into alternating word/whitespace-and-punctuation
// tokens, keeping every token (including separators) so the line can be
// re-rendered with exact original spacing - only the word tokens become
// individually clickable.
function splitIntoWords(text) {
  return text.split(/(\s+)/)
}

function isClickableWord(token) {
  return /\w/.test(token)
}

function handleWordClick(token) {
  const cleaned = token.replace(/^[^\w']+|[^\w']+$/g, '')
  if (!cleaned) return
  engine.pause()
  wordInfoPopup.value?.show(cleaned, { playWordOnOpen: true })
}

watch(subtitleCues, (cues) => {
  if (cues.length > 0 && !subtitlePollInterval) {
    subtitlePollInterval = setInterval(() => {
      currentTimeSeconds.value = engine.getCurrentTime()
    }, SUBTITLE_POLL_MS)
  }
})

async function loadSubtitlesIfAvailable() {
  if (!(await isNativeExpServerAvailable())) {
    console.log('[YT Shadowing] NativeExpServer not available, skipping subtitles')
    return
  }
  const data = await fetchSubtitles(props.url, 'en')
  if (data) {
    subtitleCues.value = data.cues
    console.log('[YT Shadowing] subtitleCues set, count:', data.cues.length)
  } else {
    console.log('[YT Shadowing] no subtitles returned for', props.url)
  }
}

// While mid-hold on the segment Capture button, while the Yes/No validation
// overlay is up, or while a Shadow session is running, every other control
// is disabled - only that hold's release, a Yes/No click, or Shadow
// finishing moves things on.
const controlsDisabled = computed(
  () => !isReady.value || isCapturing.value || !!captureRange.value || isShadowing.value,
)

const isMicBusy = computed(() => micState.phase !== 'idle' && micState.phase !== 'error')

// Seeking via a transcript line shouldn't be possible while capturing (held
// or already validated into a loop), manually recording, or shadowing - all
// cases where jumping the video elsewhere would fight with what's currently
// happening. Unlike controlsDisabled, this also blocks during isLoopingMode
// itself, not just the press-and-hold - the loop's own seek/speed controls
// were deliberately left active during a loop, but transcript links aren't.
const subtitleSeekDisabled = computed(
  () => controlsDisabled.value || isMicBusy.value || isLoopingMode.value,
)

const recordButtonLabel = computed(() => {
  switch (micState.phase) {
    case 'requesting-permission':
      return 'Requesting mic…'
    case 'recording':
      return 'Recording…'
    case 'playing':
      return 'Playing back…'
    default:
      return 'Record'
  }
})

// Manual Record always pauses on start, but should only resume on its own -
// once the recording is stopped and its instant playback finishes - if the
// video was actually playing when the button was pressed.
let wasPlayingBeforeRecord = false
let recordSessionActive = false

function toggleRecording() {
  if (micState.phase === 'recording') {
    micEngine.stop()
    return
  }
  if (micState.phase === 'idle' || micState.phase === 'error') {
    wasPlayingBeforeRecord = state.isPlaying
    recordSessionActive = true
    engine.pause()
    micEngine.start()
  }
}

watch(
  () => micState.phase,
  (phase) => {
    if (!recordSessionActive) return
    if (phase === 'idle' || phase === 'error') {
      recordSessionActive = false
      if (wasPlayingBeforeRecord) engine.play()
    }
  },
)

const isCapturing = ref(false)
const captureRange = ref(null)
const isLoopingMode = ref(false)
const loopRange = ref(null)
let captureStartSeconds = 0

const captureButtonLabel = computed(() => {
  if (isCapturing.value) return 'Capturing…'
  if (isLoopingMode.value) return 'End Capture'
  return 'Capture'
})

// Capture doesn't record anything - the video keeps playing normally while
// held. On release we just remember the start/end timestamps and pause, so
// the range can be reviewed before deciding what to do with it. Ignored
// while looping - "End Capture" is a plain click, handled below. Shared by
// both the pointer handlers and the 'c' keyboard shortcut.
function startCaptureHold() {
  if (isLoopingMode.value || !isReady.value) return
  captureRange.value = null
  captureStartSeconds = engine.getCurrentTime()
  isCapturing.value = true
}

function endCaptureHold() {
  if (isLoopingMode.value || !isCapturing.value) return
  isCapturing.value = false
  const endSeconds = engine.getCurrentTime()
  engine.pause()
  captureRange.value = { start: captureStartSeconds, end: endSeconds }
}

function handleCaptureStart(event) {
  if (isLoopingMode.value || !isReady.value) return
  event.target.setPointerCapture?.(event.pointerId)
  startCaptureHold()
}

function handleCaptureEnd() {
  endCaptureHold()
}

function handleCaptureCancel() {
  isCapturing.value = false
}

function handleCaptureClick() {
  if (isLoopingMode.value) handleEndCapture()
}

function handleValidateCapture() {
  loopRange.value = captureRange.value
  captureRange.value = null
  isLoopingMode.value = true
  engine.startLoop(loopRange.value.start, loopRange.value.end)
}

function handleDiscardCapture() {
  captureRange.value = null
  engine.play()
}

// Leaves the loop, continuing playback forward from where it left off.
function handleEndCapture() {
  const range = loopRange.value
  engine.stopLoop()
  isLoopingMode.value = false
  loopRange.value = null
  if (range) {
    engine.seekTo(range.end)
    engine.play()
  }
}

const SHADOW_REPEAT_COUNT = 2
const SHADOW_WINDOW_SECONDS = 4

const isShadowing = ref(false)
let shadowAborted = false
let shadowStopRequested = false

// Unlike the other controls, the Shadow button stays enabled while it's
// running - clicking it again requests an early stop instead of being
// blocked outright.
const shadowButtonDisabled = computed(
  () => !isReady.value || isCapturing.value || !!captureRange.value,
)

const shadowButtonLabel = computed(() => (isShadowing.value ? 'Shadowing…' : 'Shadow'))

// One play/beep/record/beep/playback/beep cycle, repeated SHADOW_REPEAT_COUNT
// times over a fixed start..end range - shared by both the in-capture and
// outside-capture Shadow flows below. Bails out early, mid-cycle, the
// moment a stop is requested or the screen is torn down.
async function runShadowCycles(startSeconds, endSeconds, recordSeconds) {
  for (let i = 0; i < SHADOW_REPEAT_COUNT && !shadowAborted && !shadowStopRequested; i++) {
    await engine.playRangeOnce(startSeconds, endSeconds)
    if (shadowAborted || shadowStopRequested) break
    await micEngine.playBeep()
    if (shadowAborted || shadowStopRequested) break
    const blob = await micEngine.recordFor(recordSeconds)
    if (shadowAborted || shadowStopRequested) break
    await micEngine.playBeep()
    if (shadowAborted || shadowStopRequested) break
    if (blob) await micEngine.playBlob(blob)
    if (shadowAborted || shadowStopRequested) break
    await micEngine.playBeep()
  }
}

// Inside a validated capture: shadows exactly that range, then stays paused
// there, still in capture mode - a manual Play or another Shadow run picks
// up from there. Outside a capture: shadows consecutive fixed-size windows
// starting from the current position (current..+5s, then +5..+10s, and so
// on), advancing automatically until the video ends or the session is
// stopped - it never plays a final window shorter than SHADOW_WINDOW_SECONDS.
// Either way, a click (or the 's' shortcut) while already running requests
// an early stop, unwound through the same end-of-session cleanup as
// finishing naturally.
async function handleShadow() {
  if (isShadowing.value) {
    shadowStopRequested = true
    return
  }

  isShadowing.value = true
  shadowAborted = false
  shadowStopRequested = false

  if (isLoopingMode.value) {
    const range = loopRange.value
    if (!range) {
      isShadowing.value = false
      return
    }

    engine.stopLoop()
    engine.pause()
    const recordSeconds = (range.end - range.start) / state.playbackRate + 1
    await runShadowCycles(range.start, range.end, recordSeconds)

    isShadowing.value = false
    if (!shadowAborted) {
      engine.setLoopRange(range.start, range.end)
    }
    return
  }

  engine.pause()
  const recordSeconds = SHADOW_WINDOW_SECONDS / state.playbackRate + 1
  let windowStart = engine.getCurrentTime()

  while (!shadowAborted && !shadowStopRequested) {
    const windowEnd = windowStart + SHADOW_WINDOW_SECONDS
    const duration = engine.getDuration()
    if (duration > 0 && windowEnd > duration) break

    await runShadowCycles(windowStart, windowEnd, recordSeconds)
    windowStart = windowEnd
  }

  isShadowing.value = false
}

function formatTime(totalSeconds) {
  const minutes = Math.floor(totalSeconds / 60)
  const seconds = totalSeconds - minutes * 60
  return `${minutes}:${seconds.toFixed(1).padStart(4, '0')}`
}

// Records the video into history once its title becomes available
// (arrives asynchronously, after the player actually starts).
watch(
  () => state.videoTitle,
  (title) => {
    if (title) addToHistory(props.videoId, props.url, title)
  },
)

// The bar is split 50/50 around 1.0 so the "normal speed" point always sits
// dead-center, even though 0.5-1.0 and 1.0-2.0 are different-sized ranges.
const speedMarkerPercent = computed(() => {
  const rate = state.playbackRate
  if (rate <= 1) {
    return ((rate - MIN_PLAYBACK_RATE) / (1 - MIN_PLAYBACK_RATE)) * 50
  }
  return 50 + ((rate - 1) / (MAX_PLAYBACK_RATE - 1)) * 50
})

function isTypingTarget(target) {
  return target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement
}

function handleKeydown(event) {
  if (isTypingTarget(event.target)) return

  // 's' mirrors the Shadow button, including staying live to request an
  // early stop while a session is running - checked before the general
  // controlsDisabled guard below, same as the button's own disabled rule.
  if (event.key.toLowerCase() === 's') {
    if (event.repeat) return
    if (shadowButtonDisabled.value) return
    handleShadow()
    return
  }

  if (controlsDisabled.value) return

  if (event.code === 'Space') {
    event.preventDefault()
    engine.togglePlayPause()
    return
  }

  if (event.key === 'ArrowLeft') {
    event.preventDefault()
    if (event.shiftKey) {
      engine.changeSpeed(-PLAYBACK_RATE_STEP)
    } else {
      engine.seekBy(-SEEK_STEP_SECONDS)
    }
    return
  }

  if (event.key === 'ArrowRight') {
    event.preventDefault()
    if (event.shiftKey) {
      engine.changeSpeed(PLAYBACK_RATE_STEP)
    } else {
      engine.seekBy(SEEK_STEP_SECONDS)
    }
    return
  }

  // 'c' mirrors the Capture button: held down to capture a range, or a
  // plain press to end the loop once already in capture mode. Ignore
  // auto-repeat keydowns from the key being held so it doesn't re-trigger.
  if (event.key.toLowerCase() === 'c') {
    if (event.repeat) return
    if (isLoopingMode.value) {
      handleEndCapture()
    } else {
      startCaptureHold()
    }
    return
  }

  // 'r' mirrors the Record button: a plain press toggles start/stop.
  if (event.key.toLowerCase() === 'r') {
    if (event.repeat) return
    toggleRecording()
  }
}

function handleKeyup(event) {
  if (event.key.toLowerCase() === 'c') {
    endCaptureHold()
  }
}

onMounted(() => {
  window.addEventListener('keydown', handleKeydown)
  window.addEventListener('keyup', handleKeyup)
  document.addEventListener('selectionchange', handleSelectionChange)
  engine.loadVideo('yt-shadowing-player', props.videoId)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleKeydown)
  window.removeEventListener('keyup', handleKeyup)
  document.removeEventListener('selectionchange', handleSelectionChange)
  if (subtitlePollInterval) clearInterval(subtitlePollInterval)
  shadowAborted = true
  engine.destroy()
  micEngine.destroy()
})

function handleBack() {
  shadowAborted = true
  engine.destroy()
  micEngine.destroy()
  emit('back')
}
</script>

<style scoped>
.player-and-transcript {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1.5rem;
  width: 100%;
}

@media (min-width: 700px) {
  .player-and-transcript {
    flex-direction: row;
    align-items: flex-start;
    justify-content: center;
  }
}

.video-column {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1.5rem;
  width: 100%;
  max-width: 24rem;
}

.yt-player-wrap {
  position: relative;
  width: 100%;
  max-width: 24rem;
  aspect-ratio: 16 / 9;
}

.yt-player-wrap :deep(iframe) {
  width: 100%;
  height: 100%;
  border-radius: 0.75rem;
}

.subtitle-panel {
  width: 100%;
  max-width: 24rem;
}

@media (min-width: 700px) {
  .subtitle-panel {
    max-width: 22rem;
  }
}

.subtitle-list {
  list-style: none;
  margin: 0;
  padding: 0.5rem;
  width: 100%;
  max-height: min(70vh, 32rem);
  overflow-y: auto;
  background: var(--surface);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.75rem;
  display: flex;
  flex-direction: column;
}

.capture-selection-button {
  width: 100%;
  margin-top: 0.75rem;
  padding: 0.7rem;
  font-size: 0.9rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease, transform 0.15s ease;
}

.capture-selection-button:active {
  background: var(--surface-2);
  transform: scale(0.98);
}

.capture-selection-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.subtitle-list-item {
  display: flex;
  align-items: baseline;
  gap: 0.5rem;
  padding: 0.5rem 0.4rem;
  border-bottom: 1px solid rgba(255, 255, 255, 0.06);
  font-size: 0.85rem;
  line-height: 1.35;
  color: var(--text-dim);
  text-align: left;
}

.subtitle-list-item:last-child {
  border-bottom: none;
}

.subtitle-list-item.active {
  color: var(--text);
}

.subtitle-list-icon {
  flex-shrink: 0;
  width: 1.1rem;
  color: var(--accent-1);
  font-size: 0.7rem;
  text-align: center;
  cursor: pointer;
  opacity: 0;
  transition: opacity 0.1s ease;
  -webkit-tap-highlight-color: transparent;
}

.subtitle-list-icon.is-active {
  opacity: 1;
}

.subtitle-list-icon:hover:not(.is-active):not(.disabled) {
  opacity: 0.5;
}

.subtitle-list-icon.disabled {
  cursor: not-allowed;
}

.subtitle-list-text {
  flex: 1;
}

.subtitle-word {
  cursor: pointer;
  border-radius: 0.2rem;
  -webkit-tap-highlight-color: transparent;
}

.subtitle-word:hover {
  background: var(--surface-2);
  color: var(--text);
}

.subtitle-overlay {
  position: absolute;
  left: 0;
  right: 0;
  bottom: 0.6rem;
  display: flex;
  justify-content: center;
  padding: 0 0.5rem;
  pointer-events: none;
}

.subtitle-text {
  background: rgba(0, 0, 0, 0.75);
  color: #fff;
  font-size: 0.85rem;
  font-weight: 600;
  padding: 0.35rem 0.7rem;
  border-radius: 0.4rem;
  max-width: 100%;
  text-align: center;
}

.capture-overlay {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 1rem;
  padding: 1rem;
  background: rgba(20, 22, 26, 0.85);
  border-radius: 0.75rem;
  text-align: center;
}

.capture-overlay-text {
  margin: 0;
  font-size: 1rem;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

.capture-overlay-buttons {
  display: flex;
  gap: 0.75rem;
}

.overlay-button {
  padding: 0.6rem 1.4rem;
  font-size: 0.95rem;
  font-weight: 700;
  border: none;
  border-radius: 999px;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.overlay-button-yes {
  background: var(--accent-gradient);
  color: #fff;
}

.overlay-button-no {
  background: var(--record);
  color: #fff;
}

.capture-button {
  width: 100%;
  max-width: 20rem;
  padding: 0.9rem;
  font-size: 1rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  touch-action: none;
  transition: background 0.15s ease, transform 0.15s ease;
}

.capture-button.recording {
  background: var(--record);
  border-color: transparent;
  color: #fff;
  transform: scale(0.98);
}

.capture-button.looping {
  background: var(--accent-gradient);
  border-color: transparent;
  color: #fff;
}

.capture-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.transport-controls {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.transport-button {
  padding: 0.8rem 1.2rem;
  font-size: 1rem;
  font-weight: 600;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: transform 0.15s ease, background 0.15s ease;
}

.transport-button:active {
  background: var(--surface-2);
  transform: scale(0.96);
}

.transport-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.transport-button-main {
  min-width: 6rem;
  background: var(--accent-gradient);
  border-color: transparent;
  color: #fff;
}

.cc-button.active {
  background: var(--accent-gradient);
  border-color: transparent;
  color: #fff;
}

.speed-control {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  width: 100%;
  max-width: 20rem;
}

.speed-step-button {
  flex-shrink: 0;
  width: 2.25rem;
  height: 2.25rem;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  font-size: 1.1rem;
  font-weight: 700;
  line-height: 1;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.speed-step-button:active {
  background: var(--surface-2);
}

.speed-step-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.speed-bar {
  position: relative;
  flex: 1;
  height: 0.4rem;
  border-radius: 999px;
  background: var(--surface-2);
}

.speed-bar-center-tick {
  position: absolute;
  left: 50%;
  top: 50%;
  width: 2px;
  height: 0.9rem;
  background: var(--text-dim);
  transform: translate(-50%, -50%);
  border-radius: 1px;
}

.speed-bar-marker {
  position: absolute;
  top: 50%;
  width: 1rem;
  height: 1rem;
  border-radius: 50%;
  background: var(--accent-gradient);
  transform: translate(-50%, -50%);
  box-shadow: 0 2px 6px rgba(58, 123, 253, 0.45);
  transition: left 0.1s linear;
}

.speed-value {
  flex-shrink: 0;
  width: 3.25rem;
  font-size: 0.95rem;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
  text-align: right;
}

.mic-controls {
  display: flex;
  gap: 0.75rem;
  width: 100%;
  max-width: 20rem;
}

.record-button {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.6rem;
  padding: 0.9rem;
  font-size: 1rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease, transform 0.15s ease;
}

.record-button:active {
  transform: scale(0.98);
}

.record-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.record-button.recording {
  background: var(--record);
  border-color: transparent;
  color: #fff;
}

.record-icon {
  display: inline-flex;
  width: 1.1rem;
  height: 1.1rem;
  color: var(--record);
}

.record-button.recording .record-icon {
  color: #fff;
  animation: record-icon-pulse 1s ease-in-out infinite;
}

.record-icon svg {
  width: 100%;
  height: 100%;
}

@keyframes record-icon-pulse {
  0%,
  100% {
    opacity: 1;
  }
  50% {
    opacity: 0.35;
  }
}

.shadow-button {
  flex: 1;
  padding: 0.9rem;
  font-size: 1rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease, transform 0.15s ease;
}

.shadow-button:active {
  transform: scale(0.98);
}

.shadow-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.shadow-button.active {
  background: var(--accent-gradient);
  border-color: transparent;
  color: #fff;
}
</style>
