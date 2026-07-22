<template>
  <main class="screen yt-player-screen">
    <!-- inert while the word popup is open - nothing behind it (clicks,
         keyboard focus/activation) should be reachable until it closes. -->
    <div class="screen-content" :inert="isWordPopupOpen || null">
      <button class="back-button" @click="handleBack">&larr; Back</button>

      <h1>YT Shadowing</h1>

      <p v-if="state.error" class="error-message">{{ state.error }}</p>

      <div class="player-and-transcript">
      <div class="video-column">
        <div class="video-frame">
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

          <div class="video-control-bar">
            <div class="video-bar-scrubber" @click="handleScrubberClick">
              <div class="video-bar-scrubber-fill" :style="{ width: progressPercent + '%' }"></div>
            </div>

            <div class="video-bar-row">
              <span class="video-bar-time">{{ formatShortTime(currentTimeSeconds) }} / {{ formatShortTime(durationSeconds) }}</span>

              <div class="video-bar-transport">
                <button
                  class="video-bar-seek-button"
                  :disabled="controlsDisabled"
                  @click="engine.seekBy(-SEEK_STEP_SECONDS)"
                >
                  -{{ SEEK_STEP_SECONDS }}
                </button>
                <button
                  class="video-bar-play-button"
                  :disabled="controlsDisabled"
                  :aria-label="state.isPlaying ? 'Pause' : 'Play'"
                  @pointerdown="handlePlayPausePointerDown"
                  @pointerup="playPauseGesture.handlePointerUp"
                  @pointercancel="playPauseGesture.handlePointerCancel"
                  @click="handleTogglePlayPause"
                >
                  <PlayPauseIcon :playing="state.isPlaying" />
                </button>
                <button
                  class="video-bar-seek-button"
                  :disabled="controlsDisabled"
                  @click="engine.seekBy(SEEK_STEP_SECONDS)"
                >
                  +{{ SEEK_STEP_SECONDS }}
                </button>
              </div>

              <div class="video-bar-right-group">
                <div class="video-bar-speed" ref="speedControlRoot">
                  <button
                    class="video-bar-speed-button"
                    :disabled="controlsDisabled"
                    @click="speedPopoverOpen = !speedPopoverOpen"
                  >
                    {{ state.playbackRate.toFixed(2) }}x
                  </button>

                  <div class="speed-popover" v-if="speedPopoverOpen">
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
                  </div>
                </div>

                <button
                  class="video-bar-cc-button"
                  :class="{ active: subtitlesEnabled }"
                  :disabled="controlsDisabled"
                  @click="toggleSubtitles"
                >
                  {{ subtitlesLoading ? 'CC…' : 'CC' }}
                </button>
              </div>
            </div>

            <div class="video-bar-divider"></div>

            <div class="mic-controls">
              <RecordShadowButtons
                compact
                :record-label="recordLabel"
                :record-active="recordSessionActive && micState.phase === 'recording'"
                :record-disabled="controlsDisabled"
                :shadow-label="shadowLabel"
                :shadow-active="isShadowing"
                :shadow-disabled="shadowButtonDisabled"
                @toggle-record="handleToggleRecording"
                @shadow-click="handleShadow"
                @shadow-pointerdown="handleShadowPointerDown"
                @shadow-pointerup="handleShadowPointerUp"
                @shadow-pointercancel="handleShadowPointerCancel"
              />

              <button
                class="auto-shadow-button compact"
                :class="{ active: isAutoShadowing }"
                :disabled="autoShadowButtonDisabled"
                @click="handleAutoShadow"
              >
                <span class="shadow-icon">
                  <svg viewBox="0 0 24 24" fill="none">
                    <g transform="translate(3,1)" fill="#ff5d5d" opacity="0.55">
                      <circle cx="12" cy="8" r="3.4" />
                      <path d="M6 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
                    </g>
                    <g fill="currentColor">
                      <circle cx="9" cy="8" r="3.4" />
                      <path d="M3 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
                    </g>
                  </svg>
                </span>
                <span>Auto</span>
              </button>

              <button
                class="capture-button compact"
                :class="{ recording: isCapturing, looping: isLoopingMode }"
                :disabled="!isReady || !!captureRange || isShadowing || isAutoShadowing || handFreeModeEnabled"
                @pointerdown="handleCaptureStart"
                @pointerup="handleCaptureEnd"
                @pointercancel="handleCaptureCancel"
                @click="handleCaptureClick"
              >
                <span class="capture-icon">
                  <svg viewBox="0 0 24 24" fill="none">
                    <path
                      d="M9 5H6a1 1 0 0 0-1 1v12a1 1 0 0 0 1 1h3"
                      stroke="currentColor"
                      stroke-width="2"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                    />
                    <path
                      d="M15 5h3a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1h-3"
                      stroke="currentColor"
                      stroke-width="2"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                    />
                  </svg>
                </span>
                <span>{{ captureButtonLabel }}</span>
              </button>

              <button
                class="expand-toggle-button"
                :class="{ expanded: advancedControlsExpanded }"
                :aria-label="advancedControlsExpanded ? 'Hide advanced controls' : 'Show advanced controls'"
                @click="advancedControlsExpanded = !advancedControlsExpanded"
              >
                <svg viewBox="0 0 24 24" fill="none">
                  <path
                    d="M6 9l6 6 6-6"
                    stroke="currentColor"
                    stroke-width="2"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                  />
                </svg>
              </button>
            </div>

            <!--
              EXPERIMENTAL - Advanced/Hands-Free controls. Deliberately
              isolated: useRecordShadow.js / RecordShadowButtons.vue /
              useShiftOrLongPress.js know nothing about this and are never
              modified for it. For now the HF button only toggles
              handFreeModeEnabled (which disables nearly everything else) -
              it doesn't do anything else yet, and its label never changes.
            -->
            <div class="video-bar-divider" v-if="advancedControlsExpanded"></div>
            <div class="hands-free-row" v-if="advancedControlsExpanded">
              <button
                class="hf-button"
                :class="{ active: handFreeModeEnabled }"
                :disabled="handFreeShadowModeEnabled || handFreeShadowDoubleModeEnabled"
                @click="handFreeModeEnabled = !handFreeModeEnabled"
              >
                <span class="capture-icon">
                  <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                    <circle cx="12" cy="12" r="7" fill="currentColor" />
                  </svg>
                </span>
                <span>HF</span>
              </button>

              <button
                class="hf-button"
                :class="{ active: handFreeShadowModeEnabled }"
                :disabled="handFreeModeEnabled || handFreeShadowDoubleModeEnabled"
                @click="handFreeShadowModeEnabled = !handFreeShadowModeEnabled"
              >
                <span class="shadow-icon">
                  <svg viewBox="0 0 24 24" fill="none">
                    <g transform="translate(3,1)" fill="#ff5d5d" opacity="0.55">
                      <circle cx="12" cy="8" r="3.4" />
                      <path d="M6 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
                    </g>
                    <g fill="currentColor">
                      <circle cx="9" cy="8" r="3.4" />
                      <path d="M3 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
                    </g>
                  </svg>
                </span>
                <span>HF</span>
              </button>

              <button
                class="hf-button"
                :class="{ active: handFreeShadowDoubleModeEnabled }"
                :disabled="handFreeModeEnabled || handFreeShadowModeEnabled"
                @click="handFreeShadowDoubleModeEnabled = !handFreeShadowDoubleModeEnabled"
              >
                <span class="shadow-icon">
                  <svg viewBox="0 0 24 24" fill="none">
                    <g transform="translate(3,1)" fill="#ff5d5d" opacity="0.55">
                      <circle cx="12" cy="8" r="3.4" />
                      <path d="M6 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
                    </g>
                    <g fill="currentColor">
                      <circle cx="9" cy="8" r="3.4" />
                      <path d="M3 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
                    </g>
                  </svg>
                </span>
                <span>HF²</span>
              </button>
            </div>
          </div>
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
                  :class="{ disabled: wordClickDisabled }"
                  @click="handleWordClick(token, $event)"
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
    </div>

    <WordInfoPopup ref="wordInfoPopup" />

    <!-- EXPERIMENTAL Hands-Free Record overlay - see script section. No
         visual content by design (not even a background change beyond the
         static scrim) - a plain tap advances handsFreePhase, a long-press
         (any phase) exits. Pointer events only (no @click) so a long-press's
         eventual release doesn't also fire a duplicate tap action. -->
    <div
      class="hands-free-overlay"
      v-if="handFreeModeEnabled || handFreeShadowModeEnabled || handFreeShadowDoubleModeEnabled"
      @pointerdown="handleHandsFreeOverlayPointerDown"
      @pointerup="handleHandsFreeOverlayPointerUp"
      @pointercancel="handleHandsFreeOverlayPointerCancel"
    ></div>
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
import { fetchSubtitles, isNativeExpServerAvailable } from '../engine/nativeExpServerClient.js'
import { openReferenceSite } from '../engine/externalDictionarySites.js'
import { useRecordShadow } from '../composables/useRecordShadow.js'
import { useShiftOrLongPress } from '../composables/useShiftOrLongPress.js'
import { MicRecorderEngine } from '../engine/micRecorderEngine.js'
import WordInfoPopup from '../components/WordInfoPopup.vue'
import RecordShadowButtons from '../components/RecordShadowButtons.vue'
import PlayPauseIcon from '../components/PlayPauseIcon.vue'

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

// Shift-click/long-press on Play temporarily drops playback to 0.5x for
// that play - only meaningful on the press that actually starts playing
// (a plain click while already playing still just pauses, same as always).
// Whenever the video next pauses, by any means, the rate before that
// shift/long-press play is restored - it's a one-off slowdown, not a
// persistent speed change.
const playPauseGesture = useShiftOrLongPress()
let savedPlaybackRateBeforeSlowPlay = null

function handlePlayPausePointerDown() {
  playPauseGesture.handlePointerDown(() => !state.isPlaying)
}

function handleTogglePlayPause(event) {
  if (!state.isPlaying && playPauseGesture.consume(event)) {
    savedPlaybackRateBeforeSlowPlay = state.playbackRate
    engine.setPlaybackRate(0.5)
  }
  engine.togglePlayPause()
}

watch(
  () => state.isPlaying,
  (isPlaying) => {
    if (isPlaying || savedPlaybackRateBeforeSlowPlay === null) return
    engine.setPlaybackRate(savedPlaybackRateBeforeSlowPlay)
    savedPlaybackRateBeforeSlowPlay = null
  },
)

const {
  micState,
  micEngine,
  recordSessionActive,
  isShadowing,
  recordLabel,
  shadowLabel,
  toggleRecording: toggleRecordingShared,
  consumeWantsDouble,
  handleShadowPointerDown,
  handleShadowPointerUp,
  handleShadowPointerCancel,
} = useRecordShadow()

const PLAYBACK_POLL_MS = 100

// Best-effort only: stays empty (and the CC button just does nothing
// visible) if NativeExpServer isn't running - see nativeExpServerClient.js.
// Only fetched lazily on the first "CC" click, then cached in subtitleCues
// for the rest of the session - toggling off/on again never re-fetches.
const subtitleCues = ref([])
const subtitlesEnabled = ref(false)
const subtitlesLoading = ref(false)
let subtitlesFetchAttempted = false

// Polled continuously once the player's ready (not gated on subtitles being
// on) - drives both the transcript's activeCueIndex below and the video
// control bar's time/scrubber.
const currentTimeSeconds = ref(0)
const durationSeconds = ref(0)
let playbackPollInterval = null

watch(
  isReady,
  (ready) => {
    if (!ready || playbackPollInterval) return
    playbackPollInterval = setInterval(() => {
      currentTimeSeconds.value = engine.getCurrentTime()
      durationSeconds.value = engine.getDuration()
    }, PLAYBACK_POLL_MS)
  },
  { immediate: true },
)

// Shared by the CC button and Auto Shadow's line-by-line mode below - both
// want the same "fetch once, ever" cues data, whether or not the transcript
// is actually visible.
async function ensureSubtitlesFetched() {
  if (subtitlesFetchAttempted) return
  subtitlesFetchAttempted = true
  subtitlesLoading.value = true
  await loadSubtitlesIfAvailable()
  subtitlesLoading.value = false
}

async function toggleSubtitles() {
  if (subtitlesEnabled.value) {
    subtitlesEnabled.value = false
    return
  }

  subtitlesEnabled.value = true
  await ensureSubtitlesFetched()
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
    isAutoShadowing.value ||
    isLoopingMode.value ||
    !hasTranscriptSelection.value ||
    handFreeModeEnabled.value, // EXPERIMENTAL Hands-Free mode
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

// Drives the `inert` binding on .screen-content and the keydown guard
// below - everything behind the popup (clicks, focus, shortcuts) must be
// unreachable while it's open.
const isWordPopupOpen = computed(() => !!wordInfoPopup.value?.visible)

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

function handleWordClick(token, event) {
  if (wordClickDisabled.value) return

  const cleaned = token.replace(/^[^\w']+|[^\w']+$/g, '')
  if (!cleaned) return

  if (event?.shiftKey) {
    openReferenceSite('cambridge', cleaned)
    return
  }

  engine.pause()
  wordInfoPopup.value?.show(cleaned, { playWordOnOpen: true })
}

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
// overlay is up, or while a Shadow/Auto Shadow session is running, every
// other control is disabled - only that hold's release, a Yes/No click, or
// Shadow/Auto Shadow finishing moves things on.
const controlsDisabled = computed(
  () =>
    !isReady.value ||
    isCapturing.value ||
    !!captureRange.value ||
    isShadowing.value ||
    isAutoShadowing.value ||
    handFreeModeEnabled.value, // EXPERIMENTAL Hands-Free mode - see below
)

const isMicBusy = computed(() => micState.phase !== 'idle' && micState.phase !== 'error')

// Clicking a word to open the dictionary popup would fight with whatever
// Record/Shadow/Auto Shadow is currently doing with the mic and/or video -
// blocked (both plain and shift-click) while any of them is active.
const wordClickDisabled = computed(
  () => isMicBusy.value || isShadowing.value || isAutoShadowing.value || handFreeModeEnabled.value,
)

// Seeking via a transcript line shouldn't be possible while capturing (held
// or already validated into a loop), manually recording, or shadowing - all
// cases where jumping the video elsewhere would fight with what's currently
// happening. Unlike controlsDisabled, this also blocks during isLoopingMode
// itself, not just the press-and-hold - the loop's own seek/speed controls
// were deliberately left active during a loop, but transcript links aren't.
const subtitleSeekDisabled = computed(
  () => controlsDisabled.value || isMicBusy.value || isLoopingMode.value,
)

// Record and Shadow's mic-engine plumbing (toggle, labels, shift/long-press
// double-pass detection) all live in useRecordShadow() now - see its import
// above. What's left here is specific to *this* screen: pausing/resuming
// the video around a recording, and Shadow's own record-and-rehear sequence
// (see toggleManualShadow/rehearOriginalAudio below).
let wasPlayingBeforeRecord = false
let manualShadowSessionActive = false
let rehearOriginalPosition = 0
let rehearRecordStartedAt = 0
let rehearVideoSecondsBack = 0
let manualShadowDoubleMode = false
let manualShadowInSecondLoop = false

function handleToggleRecording() {
  toggleRecordingShared(
    () => {
      wasPlayingBeforeRecord = state.isPlaying
      engine.pause()
    },
    () => {
      if (wasPlayingBeforeRecord) engine.play()
    },
  )
}

// Shadow's own record-and-rehear toggle, outside capture mode: press to
// record, press again (no shift needed) to stop - then, once your own
// recording's automatic playback finishes, it rewinds the video by
// (recording duration * current speed + 0.25s) from wherever it was
// paused, replays that original segment, and only then restores whatever
// play/pause state the video was actually in before you started - so you
// hear yourself, then hear the source you were shadowing, right after each
// other. Shift/long-press on the *first* press requests a second pass (see
// rehearOriginalAudio) - the shift/long-press is only read at that first
// press; every press after that is a plain stop, same as a single pass.
// While the automatic second-pass recording is running, stopping it early
// doesn't recompute rehearVideoSecondsBack - that duration (and the segment
// it replays) is fixed from the first pass so both passes rehear the exact
// same original segment. See handleShadow() for the capture-mode branch and
// the guard against starting this while it (or the elaborate flow) is still
// running.
function toggleManualShadow(event) {
  if (micState.phase === 'recording') {
    if (!manualShadowInSecondLoop) {
      const elapsedSeconds = (Date.now() - rehearRecordStartedAt) / 1000
      rehearVideoSecondsBack = elapsedSeconds * state.playbackRate + 0.25
    }
    micEngine.stop()
    return
  }

  if (isShadowing.value) return // mid own-voice playback or video rehear - ignore extra presses

  if (micState.phase === 'idle' || micState.phase === 'error') {
    isShadowing.value = true
    manualShadowDoubleMode = consumeWantsDouble(event)
    manualShadowInSecondLoop = false
    rehearOriginalPosition = engine.getCurrentTime()
    rehearRecordStartedAt = Date.now()
    wasPlayingBeforeRecord = state.isPlaying
    manualShadowSessionActive = true
    engine.pause()
    micEngine.start()
  }
}

// Single pass: replay the original segment, then done. Double (shift/long-
// press) mode instead runs a second speak/listen/replay right after,
// re-recording for exactly as long as the first pass's replay took and
// replaying that same segment again - see toggleManualShadow.
async function rehearOriginalAudio() {
  const target = Math.max(0, rehearOriginalPosition - rehearVideoSecondsBack)
  await engine.playRangeOnce(target, rehearOriginalPosition)

  if (manualShadowDoubleMode && !manualShadowInSecondLoop) {
    manualShadowInSecondLoop = true
    const blob = await micEngine.recordFor(rehearVideoSecondsBack)
    if (blob) await micEngine.playBlob(blob)
    await engine.playRangeOnce(target, rehearOriginalPosition)
    manualShadowInSecondLoop = false
  }

  if (wasPlayingBeforeRecord) engine.play()
  isShadowing.value = false
}

watch(
  () => micState.phase,
  (phase) => {
    if (phase !== 'idle' && phase !== 'error') return
    if (manualShadowSessionActive) {
      manualShadowSessionActive = false
      rehearOriginalAudio()
    }
  },
)

const isCapturing = ref(false)
const captureRange = ref(null)
const isLoopingMode = ref(false)
const loopRange = ref(null)
let captureStartSeconds = 0

// "Cap" covers both idle and mid-hold (distinguished visually via the
// .recording class's red background/scale, not by text); "End" once a
// range is committed and looping.
const captureButtonLabel = computed(() => (isLoopingMode.value ? 'End' : 'Cap'))

// Capture doesn't record anything - the video keeps playing normally while
// held. On release we just remember the start/end timestamps and pause, so
// the range can be reviewed before deciding what to do with it. Ignored
// while looping - "End" is a plain click, handled below. Shared by both the
// pointer handlers and the 'c' keyboard shortcut. Always (re)starts
// playback on press - holding on a paused frame would capture nothing.
function startCaptureHold() {
  if (isLoopingMode.value || !isReady.value) return
  captureRange.value = null
  captureStartSeconds = engine.getCurrentTime()
  isCapturing.value = true
  engine.play()
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

// isShadowing (from useRecordShadow) covers Shadow's own two behaviors
// (elaborate in-capture, or the manual record-and-rehear toggle outside
// capture, both setting the same flag); isAutoShadowing is the separate,
// always-outside-capture-only window-advancing behavior now living on its
// own button. Kept as two flags (not one) so each button's disabled/label
// state only reflects its own activity. shadowLabel (from useRecordShadow)
// already covers both the manual toggle and the in-capture cycle identically
// since both pass through the same recording/playback/rehear stages.
const isAutoShadowing = ref(false)
let shadowLoopAborted = false
let shadowLoopStopRequested = false

// Unlike the other controls, Shadow stays enabled while it's running -
// clicking it again requests an early stop (elaborate flow) or stops the
// recording (manual flow) instead of being blocked outright. Blocked while
// Auto Shadow is running since they'd fight over the same mic/video.
const shadowButtonDisabled = computed(
  () =>
    !isReady.value ||
    isCapturing.value ||
    !!captureRange.value ||
    isAutoShadowing.value ||
    handFreeModeEnabled.value, // EXPERIMENTAL Hands-Free mode
)

const autoShadowButtonDisabled = computed(
  () =>
    !isReady.value ||
    isCapturing.value ||
    !!captureRange.value ||
    isLoopingMode.value ||
    isShadowing.value ||
    handFreeModeEnabled.value, // EXPERIMENTAL Hands-Free mode
)

// One play/beep/record/beep/playback/beep cycle, repeated `repeatCount`
// times over a fixed start..end range - shared by the in-capture Shadow flow
// and Auto Shadow below. Auto Shadow always passes SHADOW_REPEAT_COUNT (its
// own default here too) since it has no shift/long-press gesture of its
// own; in-capture Shadow passes 1 or 2 based on that gesture, matching the
// other two Shadow flows - see handleInCaptureShadow. Bails out early,
// mid-cycle, the moment a stop is requested or the screen is torn down.
async function runShadowCycles(startSeconds, endSeconds, recordSeconds, repeatCount = SHADOW_REPEAT_COUNT) {
  for (let i = 0; i < repeatCount && !shadowLoopAborted && !shadowLoopStopRequested; i++) {
    await engine.playRangeOnce(startSeconds, endSeconds)
    if (shadowLoopAborted || shadowLoopStopRequested) break
    await micEngine.playBeep()
    if (shadowLoopAborted || shadowLoopStopRequested) break
    const blob = await micEngine.recordFor(recordSeconds)
    if (shadowLoopAborted || shadowLoopStopRequested) break
    await micEngine.playBeep()
    if (shadowLoopAborted || shadowLoopStopRequested) break
    if (blob) await micEngine.playBlob(blob)
    if (shadowLoopAborted || shadowLoopStopRequested) break
    await micEngine.playBeep()
  }
}

// In capture mode: single (or double, see handleInCaptureShadow) pass over
// the captured range. Outside capture mode: delegates to the manual
// record-and-rehear toggle instead - see toggleManualShadow. Shift+click (or
// Shift+S, or a long-press via handleShadowPointerDown) requests the double
// pass in both cases; only read on the press that actually starts a fresh
// session.
function handleShadow(event) {
  if (!isLoopingMode.value) {
    toggleManualShadow(event)
    return
  }

  if (isShadowing.value) {
    shadowLoopStopRequested = true
    return
  }

  handleInCaptureShadow(event)
}

// Shadows the captured range once, then stays paused there, still in
// capture mode - a manual Play or another Shadow run picks up from there.
// Shift-click/long-press runs it twice back-to-back instead, same
// convention as the outside-capture and word-popup Shadow flows.
async function handleInCaptureShadow(event) {
  const range = loopRange.value
  if (!range) return

  const wantsDouble = consumeWantsDouble(event)
  isShadowing.value = true
  shadowLoopAborted = false
  shadowLoopStopRequested = false

  engine.stopLoop()
  engine.pause()
  const recordSeconds = (range.end - range.start) / state.playbackRate + 1
  await runShadowCycles(range.start, range.end, recordSeconds, wantsDouble ? 2 : 1)

  isShadowing.value = false
  if (!shadowLoopAborted) {
    engine.setLoopRange(range.start, range.end)
  }
}

// The first subtitle cue that's still current or upcoming at the given
// time - i.e. the cue to start line-by-line Auto Shadow from. Returns -1 if
// time is past every cue's (clamped) end, meaning there's nothing left to
// shadow.
function findStartCueIndex(currentTime) {
  const cues = subtitleCues.value
  for (let i = 0; i < cues.length; i++) {
    const range = getCueRange(i)
    if (range.end > currentTime) return i
  }
  return -1
}

// Line-by-line Auto Shadow: same play/beep/record/beep/playback/beep cycle
// (twice, via runShadowCycles) as the fixed-window fallback below, but each
// "window" is one subtitle cue instead of a fixed 4s slice, and the record
// duration matches that cue's own (speed-adjusted) length instead of a
// fixed one. Stops once it runs out of cues, even if the video itself has
// more content after the last one.
async function runAutoShadowByLines(startIndex) {
  let index = startIndex
  while (!shadowLoopAborted && !shadowLoopStopRequested && index < subtitleCues.value.length) {
    const range = getCueRange(index)
    const recordSeconds = (range.end - range.start) / state.playbackRate + 1
    await runShadowCycles(range.start, range.end, recordSeconds)
    index++
  }
}

// Fixed-window fallback (no usable subtitle cues): shadows consecutive
// fixed-size windows starting from the current position (current..+4s, then
// +4..+8s, and so on), advancing automatically until the video ends or the
// session is stopped - it never plays a final window shorter than
// SHADOW_WINDOW_SECONDS.
async function runAutoShadowByWindows() {
  const recordSeconds = SHADOW_WINDOW_SECONDS / state.playbackRate + 1
  let windowStart = engine.getCurrentTime()

  while (!shadowLoopAborted && !shadowLoopStopRequested) {
    const windowEnd = windowStart + SHADOW_WINDOW_SECONDS
    const duration = engine.getDuration()
    if (duration > 0 && windowEnd > duration) break

    await runShadowCycles(windowStart, windowEnd, recordSeconds)
    windowStart = windowEnd
  }
}

// Never available in capture mode (see autoShadowButtonDisabled) - a click
// (no keyboard shortcut) while already running requests an early stop,
// unwound through the same end-of-session cleanup as finishing naturally.
// Prefers subtitle-cue lines over fixed windows: ensures subtitles are
// fetched (silently - doesn't turn on the CC transcript/overlay) before
// deciding, then falls back to fixed windows only if no cues are available
// at all (NativeExpServer not running, or this video has no captions).
async function handleAutoShadow() {
  if (isAutoShadowing.value) {
    shadowLoopStopRequested = true
    return
  }

  isAutoShadowing.value = true
  shadowLoopAborted = false
  shadowLoopStopRequested = false

  engine.pause()
  await ensureSubtitlesFetched()

  if (subtitleCues.value.length > 0) {
    const startIndex = findStartCueIndex(engine.getCurrentTime())
    if (startIndex !== -1) await runAutoShadowByLines(startIndex)
  } else {
    await runAutoShadowByWindows()
  }

  isAutoShadowing.value = false
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

// Whole-seconds "m:ss" for the video control bar - unlike formatTime()
// above (which keeps a decimal for the capture-range validation text),
// there's no need for sub-second precision here.
function formatShortTime(totalSeconds) {
  const safeSeconds = Number.isFinite(totalSeconds) ? Math.max(0, totalSeconds) : 0
  const minutes = Math.floor(safeSeconds / 60)
  const seconds = Math.floor(safeSeconds % 60)
  return `${minutes}:${String(seconds).padStart(2, '0')}`
}

const progressPercent = computed(() => {
  if (durationSeconds.value <= 0) return 0
  return Math.min(100, (currentTimeSeconds.value / durationSeconds.value) * 100)
})

function handleScrubberClick(event) {
  if (controlsDisabled.value || durationSeconds.value <= 0) return
  const rect = event.currentTarget.getBoundingClientRect()
  const fraction = Math.min(1, Math.max(0, (event.clientX - rect.left) / rect.width))
  engine.seekTo(fraction * durationSeconds.value)
}

// The speed popover closes on a second click of its own toggle button (that
// click just flips speedPopoverOpen off again), on Escape (see handleKeydown
// below), or on any other click outside speedControlRoot entirely.
const speedPopoverOpen = ref(false)
const speedControlRoot = ref(null)

function handleSpeedPopoverOutsideClick(event) {
  if (speedControlRoot.value && !speedControlRoot.value.contains(event.target)) {
    speedPopoverOpen.value = false
  }
}

watch(speedPopoverOpen, (open) => {
  if (open) {
    document.addEventListener('click', handleSpeedPopoverOutsideClick, true)
  } else {
    document.removeEventListener('click', handleSpeedPopoverOutsideClick, true)
  }
})

// ============================================================================
// EXPERIMENTAL - Advanced/Hands-Free controls. Deliberately isolated:
// useRecordShadow.js / RecordShadowButtons.vue / useShiftOrLongPress.js know
// nothing about this and are never modified for it. `handFreeModeEnabled`
// is threaded into existing :disabled computeds/the keydown guard below
// (the one exception to "add new, don't modify existing" - there's no
// other sane way to gate a cross-cutting "disable nearly everything" flag),
// but no other button's own logic, label, or click handler is touched.
// The HF button itself does nothing beyond toggling this flag for now, and
// its label is static - never changes based on this flag or anything else.
// ============================================================================
const advancedControlsExpanded = ref(false)
const handFreeModeEnabled = ref(false)
// "HF" (Shadow icon) and "HF²" (Shadow icon) - same tap-anywhere overlay
// mechanism as handFreeModeEnabled above. handFreeShadowDoubleModeEnabled
// forces the double-pass on every cycle; handFreeShadowModeEnabled always
// stays single-pass. Mutually exclusive with each other and with
// handFreeModeEnabled - see the :disabled bindings on all three buttons.
const handFreeShadowModeEnabled = ref(false)
const handFreeShadowDoubleModeEnabled = ref(false)

// A full, deliberately independent copy of the outside-capture Shadow flow
// (toggleManualShadow/rehearOriginalAudio above) - its own MicRecorderEngine
// instance and its own plain (non-reactive - nothing here drives any UI)
// state, never useRecordShadow's shared micEngine/micState/isShadowing.
// Kept as a full copy rather than a shared helper on purpose: the real
// Shadow button must never react to hands-free activity (sharing state
// with it before caused the visible button to light up/relabel itself), and
// this is expected to evolve differently from the visible flow over time.
let handsFreeShadowActive = false
let handsFreeShadowSessionActive = false
let handsFreeShadowWasPlayingBeforeRecord = false
let handsFreeShadowOriginalPosition = 0
let handsFreeShadowRecordStartedAt = 0
let handsFreeShadowVideoSecondsBack = 0
let handsFreeShadowDoubleMode = false
let handsFreeShadowInSecondLoop = false

function handleHandsFreeShadowMicChange(snapshot) {
  if (snapshot.phase !== 'idle' && snapshot.phase !== 'error') return
  if (!handsFreeShadowSessionActive) return
  handsFreeShadowSessionActive = false
  rehearHandsFreeShadowOriginal()
}

const handsFreeShadowMicEngine = new MicRecorderEngine({ onChange: handleHandsFreeShadowMicChange })

// Same shape as toggleManualShadow, but wantsDouble is passed in directly
// (which of the two buttons opened the overlay) instead of read off a
// shift-key/long-press gesture - there's no button click event to read it
// from here.
function handFreeShadow(wantsDouble) {
  if (handsFreeShadowMicEngine.phase === 'recording') {
    if (!handsFreeShadowInSecondLoop) {
      const elapsedSeconds = (Date.now() - handsFreeShadowRecordStartedAt) / 1000
      handsFreeShadowVideoSecondsBack = elapsedSeconds * state.playbackRate + 0.25
    }
    handsFreeShadowMicEngine.stop()
    return
  }

  if (handsFreeShadowActive) return // mid rehear - ignore extra taps

  if (handsFreeShadowMicEngine.phase === 'idle' || handsFreeShadowMicEngine.phase === 'error') {
    handsFreeShadowActive = true
    handsFreeShadowDoubleMode = wantsDouble
    handsFreeShadowInSecondLoop = false
    handsFreeShadowOriginalPosition = engine.getCurrentTime()
    handsFreeShadowRecordStartedAt = Date.now()
    handsFreeShadowWasPlayingBeforeRecord = state.isPlaying
    handsFreeShadowSessionActive = true
    engine.pause()
    handsFreeShadowMicEngine.start()
  }
}

async function rehearHandsFreeShadowOriginal() {
  const target = Math.max(0, handsFreeShadowOriginalPosition - handsFreeShadowVideoSecondsBack)
  await engine.playRangeOnce(target, handsFreeShadowOriginalPosition)

  if (handsFreeShadowDoubleMode && !handsFreeShadowInSecondLoop) {
    handsFreeShadowInSecondLoop = true
    const blob = await handsFreeShadowMicEngine.recordFor(handsFreeShadowVideoSecondsBack)
    if (blob) await handsFreeShadowMicEngine.playBlob(blob)
    await engine.playRangeOnce(target, handsFreeShadowOriginalPosition)
    handsFreeShadowInSecondLoop = false
  }

  if (handsFreeShadowWasPlayingBeforeRecord) engine.play()
  handsFreeShadowActive = false
}

// Hands-Free Record: while handFreeModeEnabled is on, a full-viewport
// overlay (see template) captures every click/tap as "the press" - a direct
// stand-in for the physical button press an earlier (abandoned) AirPods-
// based design could only detect indirectly. Own independent
// MicRecorderEngine instance, never useRecordShadow's shared one (same
// "fine to reuse the class, never the instance" isolation rule as before).
// No visual/audio feedback anywhere in this cycle, by design - the whole
// point is not looking at the screen.
let handsFreePhase = 'idle' // idle | record | replay
let handsFreeRecordStartPosition = 0

function handleHandsFreeMicChange(snapshot) {
  if (handsFreePhase !== 'replay') return
  if (snapshot.phase !== 'idle' && snapshot.phase !== 'error') return
  handsFreePhase = 'idle'
  engine.seekTo(handsFreeRecordStartPosition)
  engine.play()
}

const handsFreeMicEngine = new MicRecorderEngine({ onChange: handleHandsFreeMicChange })

// idle -> record: pause, remember position, start recording.
// record -> replay: stop recording (auto-plays the recording back, per
// MicRecorderEngine's own start()/stop() contract); video stays paused.
// replay: a tap here is a no-op - playback just keeps going regardless.
function handleHandsFreeOverlayClick() {
  if (handsFreePhase === 'idle') {
    handsFreePhase = 'record'
    handsFreeRecordStartPosition = engine.getCurrentTime()
    engine.pause()
    handsFreeMicEngine.start()
    return
  }

  if (handsFreePhase === 'record') {
    handsFreePhase = 'replay'
    handsFreeMicEngine.stop()
  }
}

// Single tap dispatcher, shared by all three Hands-Free overlays (only one
// is ever active/rendered at a time - see the template's v-if and the
// buttons' mutual :disabled bindings) - routes to whichever mode is active.
function handleHandsFreeOverlayTap() {
  if (handFreeModeEnabled.value) {
    handleHandsFreeOverlayClick()
    return
  }
  if (handFreeShadowModeEnabled.value) {
    handFreeShadow(false)
    return
  }
  if (handFreeShadowDoubleModeEnabled.value) {
    handFreeShadow(true)
  }
}

// A long-press anywhere on the overlay, in any phase, exits whichever mode
// is active and resets it - the watchers below tear down the relevant
// engine state, so re-enabling any of the three always starts fresh.
// Deliberately its own standalone timer, not useShiftOrLongPress - that
// composable is additive (click still fires, plus something extra); this
// needs to be exclusive (long-press replaces the tap's normal action
// entirely), and stays simpler done directly.
const HANDS_FREE_LONG_PRESS_MS = 500
let handsFreeLongPressTimer = null
let handsFreeLongPressFired = false

function clearHandsFreeLongPressTimer() {
  if (handsFreeLongPressTimer) {
    clearTimeout(handsFreeLongPressTimer)
    handsFreeLongPressTimer = null
  }
}

function exitAllHandsFreeModes() {
  handFreeModeEnabled.value = false
  handFreeShadowModeEnabled.value = false
  handFreeShadowDoubleModeEnabled.value = false
}

function handleHandsFreeOverlayPointerDown() {
  handsFreeLongPressFired = false
  handsFreeLongPressTimer = setTimeout(() => {
    handsFreeLongPressTimer = null
    handsFreeLongPressFired = true
    exitAllHandsFreeModes()
  }, HANDS_FREE_LONG_PRESS_MS)
}

function handleHandsFreeOverlayPointerUp() {
  clearHandsFreeLongPressTimer()
  if (handsFreeLongPressFired) {
    handsFreeLongPressFired = false
    return
  }
  handleHandsFreeOverlayTap()
}

function handleHandsFreeOverlayPointerCancel() {
  clearHandsFreeLongPressTimer()
  handsFreeLongPressFired = false
}

// Shared cleanup for every way handFreeModeEnabled can turn off - the
// long-press exit above, or anything else that might flip it later.
watch(handFreeModeEnabled, (enabled) => {
  if (enabled) return
  handsFreePhase = 'idle'
  handsFreeMicEngine.destroy()
})

// Same cleanup, for whichever of the two Shadow overlays turns off - resets
// this feature's own independent state/engine (never useRecordShadow's), so
// an exit mid-recording/mid-playback stops the mic immediately rather than
// leaving it running invisibly.
watch([handFreeShadowModeEnabled, handFreeShadowDoubleModeEnabled], ([single, double]) => {
  if (single || double) return
  handsFreeShadowActive = false
  handsFreeShadowSessionActive = false
  handsFreeShadowInSecondLoop = false
  handsFreeShadowMicEngine.destroy()
})

function isTypingTarget(target) {
  return target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement
}

function handleKeydown(event) {
  if (isTypingTarget(event.target)) return

  // While the word popup is open, none of this screen's shortcuts should
  // fire - it has its own separate Escape-to-close handler.
  if (isWordPopupOpen.value) return

  // EXPERIMENTAL Hands-Free mode - see above. Locks out every shortcut
  // while active, same as it locks out nearly every button.
  if (handFreeModeEnabled.value) return

  if (event.key === 'Escape' && speedPopoverOpen.value) {
    speedPopoverOpen.value = false
    return
  }

  // 's' mirrors the Shadow button, including staying live to request an
  // early stop while a session is running - checked before the general
  // controlsDisabled guard below, same as the button's own disabled rule.
  // Shift+S mirrors shift-clicking it (see handleShadow).
  if (event.key.toLowerCase() === 's') {
    if (event.repeat) return
    if (shadowButtonDisabled.value) return
    handleShadow(event)
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

  // 'r' mirrors the Record button: a plain press toggles start/stop. No
  // shift variant anymore - see toggleManualShadow for that flow, now on
  // Shadow instead.
  if (event.key.toLowerCase() === 'r') {
    if (event.repeat) return
    handleToggleRecording()
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
  if (playbackPollInterval) clearInterval(playbackPollInterval)
  document.removeEventListener('click', handleSpeedPopoverOutsideClick, true)
  shadowLoopAborted = true
  engine.destroy()
  micEngine.destroy()
  handsFreeMicEngine.destroy()
  handsFreeShadowMicEngine.destroy()
})

function handleBack() {
  shadowLoopAborted = true
  engine.destroy()
  micEngine.destroy()
  handsFreeMicEngine.destroy()
  handsFreeShadowMicEngine.destroy()
  emit('back')
}
</script>

<style scoped>
/* Mirrors .screen's own flex layout (style.css) - this wrapper exists
   purely to give the `inert` binding a single target, and shouldn't change
   how back-button/h1/error/player-and-transcript are laid out. */
.screen-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1.5rem;
  width: 100%;
}

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

.video-frame {
  display: flex;
  flex-direction: column;
  width: 100%;
  max-width: 24rem;
}

.yt-player-wrap {
  position: relative;
  width: 100%;
  aspect-ratio: 16 / 9;
}

.yt-player-wrap :deep(iframe) {
  width: 100%;
  height: 100%;
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

.subtitle-word:hover:not(.disabled) {
  background: var(--surface-2);
  color: var(--text);
}

.subtitle-word.disabled {
  cursor: not-allowed;
  opacity: 0.5;
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
  flex: 1 1 auto;
  min-width: 6rem;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  padding: 0.8rem 0.6rem;
  font-size: 0.9rem;
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

.capture-icon {
  display: inline-flex;
  width: 1.1rem;
  height: 1.1rem;
  flex-shrink: 0;
}

.capture-icon svg {
  width: 100%;
  height: 100%;
}

.capture-button.recording {
  background: var(--record);
  border-color: transparent;
  color: #fff;
  transform: scale(0.98);
}

.capture-button.looping {
  background: var(--record);
  border-color: transparent;
  color: #fff;
}

.capture-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

/* Matches RecordShadowButtons.vue's own .compact rules - same treatment so
   all 4 buttons in this row look consistently sized. */
.capture-button.compact {
  flex: 0 0 auto;
  min-width: 0;
  padding: 0.35rem 0.6rem;
  font-size: 0.72rem;
  gap: 0.3rem;
}

.capture-button.compact .capture-icon {
  width: 0.9rem;
  height: 0.9rem;
}

.video-bar-cc-button {
  flex-shrink: 0;
  padding: 0.3rem 0.55rem;
  font-size: 0.75rem;
  font-weight: 700;
  border: none;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.1);
  color: #fff;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease;
}

.video-bar-cc-button:active {
  background: rgba(255, 255, 255, 0.22);
}

.video-bar-cc-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.video-bar-cc-button.active {
  background: var(--accent-gradient);
  color: #fff;
}

.video-control-bar {
  width: 100%;
  background: #000;
}

.video-bar-divider {
  height: 1px;
  margin: 0 0.75rem;
  background: rgba(255, 255, 255, 0.15);
}

.video-bar-scrubber {
  position: relative;
  width: 100%;
  height: 0.35rem;
  background: rgba(255, 255, 255, 0.15);
  cursor: pointer;
}

.video-bar-scrubber-fill {
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  background: var(--accent-gradient);
}

/* 1fr/auto/1fr so the transport group truly centers on the whole bar,
   regardless of how wide the time text or the right-hand group are - the
   two 1fr columns always balance equally around the fixed-width middle
   one, which plain flex+space-between can't guarantee. */
.video-bar-row {
  display: grid;
  grid-template-columns: 1fr auto 1fr;
  align-items: center;
  gap: 0.5rem;
  padding: 0.5rem 0.75rem;
}

.video-bar-right-group {
  display: flex;
  align-items: center;
  gap: 0.4rem;
  justify-self: end;
}

.video-bar-time {
  justify-self: start;
  flex-shrink: 0;
  color: #fff;
  font-size: 0.6rem;
  font-variant-numeric: tabular-nums;
  opacity: 0.85;
}

.video-bar-transport {
  display: flex;
  align-items: center;
  gap: 0.4rem;
}

.video-bar-seek-button {
  flex-shrink: 0;
  padding: 0.3rem 0.5rem;
  font-size: 0.75rem;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
  border: none;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.1);
  color: #fff;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease;
}

.video-bar-seek-button:active {
  background: rgba(255, 255, 255, 0.22);
}

.video-bar-seek-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.video-bar-play-button {
  flex-shrink: 0;
  width: 2.1rem;
  height: 2.1rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border: none;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.14);
  color: #fff;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease, transform 0.15s ease;
}

.video-bar-play-button:active {
  transform: scale(0.92);
}

.video-bar-play-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.video-bar-play-button :deep(svg) {
  width: 0.9rem;
  height: 0.9rem;
}

.video-bar-speed {
  position: relative;
  flex-shrink: 0;
}

.video-bar-speed-button {
  padding: 0.3rem 0.55rem;
  font-size: 0.75rem;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
  border: none;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.1);
  color: #fff;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease;
}

.video-bar-speed-button:active {
  background: rgba(255, 255, 255, 0.22);
}

.video-bar-speed-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.speed-popover {
  position: absolute;
  bottom: calc(100% + 0.5rem);
  right: 0;
  display: flex;
  align-items: center;
  gap: 0.5rem;
  width: 11rem;
  padding: 0.6rem 0.75rem;
  background: var(--surface);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 0.75rem;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
  z-index: 5;
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

.mic-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.4rem;
  width: 100%;
  padding: 0.5rem 0.75rem;
}

.expand-toggle-button {
  flex-shrink: 0;
  margin-left: auto;
  width: 1.6rem;
  height: 1.6rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border: none;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.06);
  color: var(--text-dim);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.expand-toggle-button svg {
  width: 0.85rem;
  height: 0.85rem;
  transition: transform 0.15s ease;
}

.expand-toggle-button.expanded svg {
  transform: rotate(180deg);
}

/* EXPERIMENTAL - Advanced/Hands-Free controls. Kept visually distinct
   enough (amber accent) to read as "not a core feature". */
.hands-free-row {
  display: flex;
  align-items: center;
  gap: 0.4rem;
  width: 100%;
  padding: 0.5rem 0.75rem;
}

.hf-button {
  flex: 0 0 auto;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.3rem;
  padding: 0.35rem 0.6rem;
  font-size: 0.72rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease, transform 0.15s ease;
}

.hf-button:active {
  transform: scale(0.96);
}

.hf-button .capture-icon {
  width: 0.9rem;
  height: 0.9rem;
}

.hf-button .shadow-icon {
  width: 0.9rem;
  height: 0.9rem;
}

.hf-button.active {
  background: #b8860b;
  border-color: transparent;
  color: #fff;
}

/* EXPERIMENTAL - Hands-Free Record's tap-anywhere overlay. Deliberately
   featureless beyond the scrim itself - no text/icon/animation, since the
   whole point is not looking at the screen. touch-action/user-select/
   -webkit-touch-callout suppress iOS Safari's default touch-and-hold
   behavior (text selection/callout menu), which otherwise hijacks the
   long-press before our own pointer-event timer ever sees it. */
.hands-free-overlay {
  position: fixed;
  inset: 0;
  z-index: 1000;
  background: rgba(0, 0, 0, 0.6);
  cursor: pointer;
  touch-action: none;
  user-select: none;
  -webkit-user-select: none;
  -webkit-touch-callout: none;
}

/* .record-button / .shadow-button and their icon styles now live in
   RecordShadowButtons.vue - .shadow-icon stays here too since Auto Shadow
   below reuses the same icon markup directly, not via that component. */
.shadow-icon {
  display: inline-flex;
  width: 1.15rem;
  height: 1.15rem;
  flex-shrink: 0;
}

.shadow-icon svg {
  width: 100%;
  height: 100%;
}

.auto-shadow-button {
  flex: 1 1 auto;
  min-width: 6rem;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  padding: 0.8rem 0.6rem;
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

.auto-shadow-button:active {
  transform: scale(0.98);
}

.auto-shadow-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.auto-shadow-button.active {
  background: var(--accent-gradient);
  border-color: transparent;
  color: #fff;
}

/* Matches RecordShadowButtons.vue's own .compact rules - same treatment so
   all 4 buttons in this row look consistently sized. */
.auto-shadow-button.compact {
  flex: 0 0 auto;
  min-width: 0;
  padding: 0.35rem 0.6rem;
  font-size: 0.72rem;
  gap: 0.3rem;
}

.auto-shadow-button.compact .shadow-icon {
  width: 0.9rem;
  height: 0.9rem;
}
</style>
