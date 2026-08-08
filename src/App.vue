<template>
  <HomeScreen
    v-if="screen.name === 'home'"
    @select-tool="onSelectTool"
    @open-settings="goToSettings"
    @open-dictionary="handleOpenDictionary"
  />

  <SettingsScreen
    v-else-if="screen.name === 'settings'"
    @back="goHome"
    @open-logs="goToLogs"
  />

  <LogsScreen v-else-if="screen.name === 'logs'" @back="goToSettings" />

  <DurationPickerScreen
    v-else-if="screen.name === 'recorder-loop-duration'"
    title="Recorder Loop"
    @select-duration="onSelectRecorderDuration"
    @back="goHome"
  />

  <RecorderLoopSessionScreen
    v-else-if="screen.name === 'recorder-loop-session'"
    :duration="screen.duration"
    @back="goToRecorderDurationPicker"
  />

  <RobotShadowingSessionScreen
    v-else-if="screen.name === 'robot-shadowing-session'"
    @back="goHome"
  />

  <YtShadowingFormScreen
    v-else-if="screen.name === 'yt-shadowing-form'"
    @back="goHome"
    @load="onLoadYtVideo"
  />

  <YtShadowingPlayerScreen
    v-else-if="screen.name === 'yt-shadowing-player'"
    :video-id="screen.videoId"
    :url="screen.url"
    :dictionary-open="dictionaryOpen"
    @back="goToYtShadowingForm"
    @show-word="handleShowWord"
  />

  <FlashcardsSetPickerScreen
    v-else-if="screen.name === 'flashcards-set-picker'"
    @back="goHome"
    @select-set="onSelectFlashcardsSet"
  />

  <FlashcardsLearnScreen
    v-else-if="screen.name === 'flashcards-learn'"
    :set-name="screen.setName"
    :dictionary-open="dictionaryOpen"
    @back="goToFlashcardsEdit"
    @show-word="handleShowWord"
  />

  <FlashcardsReviewScreen
    v-else-if="screen.name === 'flashcards-review'"
    :set-name="screen.setName"
    :dictionary-open="dictionaryOpen"
    @back="goToFlashcardsEdit"
    @show-word="handleShowWord"
  />

  <FlashcardsPracticeScreen
    v-else-if="screen.name === 'flashcards-practice'"
    :set-name="screen.setName"
    :dictionary-open="dictionaryOpen"
    @back="goToFlashcardsEdit"
    @show-word="handleShowWord"
  />

  <FlashcardsEditScreen
    v-else-if="screen.name === 'flashcards-edit'"
    :set-name="screen.setName"
    @back="goToFlashcardsSetPicker"
    @learn="onFlashcardsAction('flashcards-learn')"
    @review="onFlashcardsAction('flashcards-review')"
    @practice="onFlashcardsAction('flashcards-practice')"
  />

  <!-- Global, single instance - deliberately outside the screen v-if/else-if
       chain above so it can overlay any screen, triggered from the Home
       menu, a transcript word click (relayed up via @show-word), or the
       app-wide keyboard shortcut below. See docs/dictionary-spec.md. -->
  <DictionaryPopup ref="dictionaryPopupRef" />

  <!-- Also global/single-instance - see composables/useToast.js. -->
  <ToastHost />
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import HomeScreen from './screens/HomeScreen.vue'
import SettingsScreen from './screens/SettingsScreen.vue'
import LogsScreen from './screens/LogsScreen.vue'
import DurationPickerScreen from './screens/DurationPickerScreen.vue'
import RecorderLoopSessionScreen from './screens/RecorderLoopSessionScreen.vue'
import RobotShadowingSessionScreen from './screens/RobotShadowingSessionScreen.vue'
import YtShadowingFormScreen from './screens/YtShadowingFormScreen.vue'
import YtShadowingPlayerScreen from './screens/YtShadowingPlayerScreen.vue'
import FlashcardsSetPickerScreen from './screens/FlashcardsSetPickerScreen.vue'
import FlashcardsLearnScreen from './screens/FlashcardsLearnScreen.vue'
import FlashcardsReviewScreen from './screens/FlashcardsReviewScreen.vue'
import FlashcardsPracticeScreen from './screens/FlashcardsPracticeScreen.vue'
import FlashcardsEditScreen from './screens/FlashcardsEditScreen.vue'
import DictionaryPopup from './components/DictionaryPopup.vue'
import ToastHost from './components/ToastHost.vue'

const screen = ref({ name: 'home' })

const dictionaryPopupRef = ref(null)
const dictionaryOpen = computed(() => !!dictionaryPopupRef.value?.visible)

function handleShowWord(word) {
  dictionaryPopupRef.value?.showWord(word, { playWordOnOpen: true })
}

function handleOpenDictionary() {
  dictionaryPopupRef.value?.showSearch()
}

// Global "open the dictionary" shortcut - Option+Shift+D, works from any
// screen. Uses event.code (not event.key) since Option held on Mac makes
// `key` report a produced special character ("∂" on a US layout) rather
// than "d" - `code` stays layout/modifier-independent. No-ops if the popup
// is already open (covers both "pressed twice" and "a word's already
// showing"), so this never steals focus from an in-progress lookup.
function handleGlobalKeydown(event) {
  if (!event.altKey || !event.shiftKey || event.code !== 'KeyD') return
  if (dictionaryPopupRef.value?.visible) return
  event.preventDefault()
  handleOpenDictionary()
}

onMounted(() => {
  window.addEventListener('keydown', handleGlobalKeydown)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleGlobalKeydown)
})

function onSelectTool(toolId) {
  if (toolId === 'recorder-loop') {
    screen.value = { name: 'recorder-loop-duration' }
  } else if (toolId === 'robot-shadowing') {
    screen.value = { name: 'robot-shadowing-session' }
  } else if (toolId === 'yt-shadowing') {
    screen.value = { name: 'yt-shadowing-form' }
  } else if (toolId === 'flashcards') {
    screen.value = { name: 'flashcards-set-picker' }
  }
}

function onSelectRecorderDuration(seconds) {
  screen.value = { name: 'recorder-loop-session', duration: seconds }
}

function onLoadYtVideo({ videoId, url }) {
  screen.value = { name: 'yt-shadowing-player', videoId, url }
}

function onSelectFlashcardsSet(setName) {
  screen.value = { name: 'flashcards-edit', setName }
}

// One handler shared by all four Flashcards sub-modes - each just needs the
// current setName carried along, so there's no per-mode logic to branch on.
function onFlashcardsAction(screenName) {
  screen.value = { name: screenName, setName: screen.value.setName }
}

function goToFlashcardsSetPicker() {
  screen.value = { name: 'flashcards-set-picker' }
}

function goToFlashcardsEdit() {
  screen.value = { name: 'flashcards-edit', setName: screen.value.setName }
}

function goHome() {
  screen.value = { name: 'home' }
}

function goToSettings() {
  screen.value = { name: 'settings' }
}

function goToLogs() {
  screen.value = { name: 'logs' }
}

function goToRecorderDurationPicker() {
  screen.value = { name: 'recorder-loop-duration' }
}

function goToYtShadowingForm() {
  screen.value = { name: 'yt-shadowing-form' }
}
</script>
