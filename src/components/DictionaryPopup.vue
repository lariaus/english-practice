<template>
  <div class="dictionary-backdrop" v-if="visible" @click.self="hide">
    <div class="dictionary-frame">
      <button class="dictionary-close" @click="hide" aria-label="Close">&times;</button>

      <form v-if="mode === 'search'" class="dictionary-search-form" @submit.prevent="handleSearchSubmit">
        <input
          v-model="searchInput"
          class="dictionary-search-input"
          type="text"
          placeholder="Search a word…"
          autofocus
        />
      </form>

      <p v-if="loading" class="dictionary-status">Loading…</p>

      <p v-else-if="mode === 'search' && !currentWord" class="dictionary-status">
        Type a word above and press Enter.
      </p>

      <template v-else>
        <div class="dictionary-header">
          <h2 class="dictionary-word">{{ currentWord }}</h2>
          <span v-if="info?.usPhonetics" class="dictionary-header-phonetic">{{ info.usPhonetics.text }}</span>
          <button
            class="dictionary-play-button dictionary-word-play-button"
            @pointerdown="playGesture.handlePointerDown()"
            @pointerup="playGesture.handlePointerUp"
            @pointercancel="playGesture.handlePointerCancel"
            @click="handlePlayWordClick"
            aria-label="Play word"
          >
            <PlayPauseIcon />
          </button>

          <RecordShadowButtons
            compact
            :record-label="recordLabel"
            :record-active="recordSessionActive && micState.phase === 'recording'"
            :record-disabled="isShadowing"
            :shadow-label="shadowLabel"
            :shadow-active="isShadowing"
            :shadow-disabled="recordSessionActive"
            @toggle-record="toggleRecording($event)"
            @record-pointerdown="handleRecordPointerDown"
            @record-pointerup="handleRecordPointerUp"
            @record-pointercancel="handleRecordPointerCancel"
            @shadow-click="handleShadowClick"
            @shadow-pointerdown="handleShadowPointerDown"
            @shadow-pointerup="handleShadowPointerUp"
            @shadow-pointercancel="handleShadowPointerCancel"
          />
        </div>

        <p v-if="micState.error" class="error-message dictionary-mic-error">{{ micState.error }}</p>

        <template v-if="info">
          <div class="dictionary-phonetics" v-if="info.phonetics.length">
            <div v-for="(p, i) in info.phonetics" :key="i" class="dictionary-phonetic">
              <span class="dictionary-phonetic-label">{{ p.label || '??' }}</span>
              <span class="dictionary-phonetic-text">{{ p.text }}</span>
              <button
                v-if="p.audio"
                class="dictionary-play-button"
                @pointerdown="playGesture.handlePointerDown()"
                @pointerup="playGesture.handlePointerUp"
                @pointercancel="playGesture.handlePointerCancel"
                @click="handlePlayPhoneticClick($event, p.audio)"
                aria-label="Play pronunciation"
              >
                <PlayPauseIcon />
              </button>
            </div>
          </div>

          <div class="dictionary-meaning" v-for="(meaning, i) in info.meanings" :key="i">
            <h3 class="dictionary-pos">{{ meaning.partOfSpeech }}</h3>
            <ol class="dictionary-definitions">
              <li v-for="(def, j) in meaning.definitions.slice(0, 2)" :key="j">
                <p class="dictionary-definition">{{ def.definition }}</p>
                <p class="dictionary-example" v-if="def.example">"{{ def.example }}"</p>
                <p class="dictionary-synonyms" v-if="def.synonyms.length">
                  syn: {{ def.synonyms.join(', ') }}
                </p>
              </li>
            </ol>
          </div>

          <p class="dictionary-attribution" v-if="info.sourceUrl">
            via <a :href="info.sourceUrl" target="_blank" rel="noopener">Wiktionary</a>
            <template v-if="info.license"> · {{ info.license }}</template>
          </p>
        </template>

        <p v-else-if="error" class="dictionary-status">{{ error }}</p>

        <div class="dictionary-external-links">
          <button class="dictionary-external-link" @click="openReferenceSite('cambridge', currentWord)">Ca</button>
          <button class="dictionary-external-link" @click="openReferenceSite('wordreference', currentWord)">Wr</button>
        </div>
      </template>
    </div>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { fetchWordInfo } from '../engine/dictionaryClient.js'
import { playAudioUrl, playWordPronunciation, playWordPronunciationTimed } from '../engine/wordAudioPlayer.js'
import { openReferenceSite } from '../engine/externalDictionarySites.js'
import { useRecordShadow } from '../composables/useRecordShadow.js'
import { useShiftOrLongPress } from '../composables/useShiftOrLongPress.js'
import RecordShadowButtons from './RecordShadowButtons.vue'
import PlayPauseIcon from './PlayPauseIcon.vue'

const visible = ref(false)
// 'word' - opened for one specific word (a transcript click), no search bar.
// 'search' - opened via the Home menu or the global shortcut: shows the
// search bar, starts blank, and stays in this mode even after a lookup (so
// you can search again) - strictly never crosses into/out of 'word' mode.
const mode = ref('word')
const searchInput = ref('')
const loading = ref(false)
const error = ref(null)
const info = ref(null)
const currentWord = ref('')

// Shift-click/long-press on any play button here plays that clip at 0.5x
// instead of normal speed - one-shot, no "restore afterward" needed like
// the video's Play/Pause has, since a word clip has no persistent state to
// revert. One shared gesture tracker for every play button in this popup -
// only one can be physically pressed at a time.
const playGesture = useShiftOrLongPress()

function handlePlayWordClick(event) {
  playWordPronunciation(currentWord.value, 'en', playGesture.consume(event) ? 0.5 : 1)
}

function handlePlayPhoneticClick(event, audioUrl) {
  playAudioUrl(audioUrl, playGesture.consume(event) ? 0.5 : 1)
}

const {
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
  destroy: destroyRecordShadow,
} = useRecordShadow()

// Set on hide(), cleared on show() - lets an in-flight Shadow pass notice
// the popup closed and stop advancing, even mid-await (e.g. still playing
// the word's own clip, before recordFor() would even start) - destroying
// the mic engine alone doesn't prevent a *not-yet-started* recordFor() from
// quietly kicking off a fresh recording after the popup is already gone.
let popupClosed = false

// One play-word / record / listen-to-yourself pass - the word's own clip is
// played up front (unlike YT Shadowing's video segment, its length is known
// as soon as it's played), and that same elapsed length + 0.25s becomes the
// recording window, so there's no separate "how long should I record"
// guess to make.
async function runWordShadowPass() {
  const elapsedSeconds = await playWordPronunciationTimed(currentWord.value)
  if (popupClosed) return
  const blob = await micEngine.recordFor(elapsedSeconds + 0.25)
  if (popupClosed) return
  if (blob) await micEngine.playBlob(blob)
}

// Shift-click/Shift+long-press requests a second pass immediately after the
// first, back-to-back - only read on the press that starts a fresh session,
// same convention as YT Shadowing's outside-capture Shadow.
async function handleShadowClick(event) {
  if (micState.phase === 'recording') {
    micEngine.stop()
    return
  }
  if (isShadowing.value) return

  if (micState.phase === 'idle' || micState.phase === 'error') {
    const wantsDouble = consumeWantsDouble(event)
    isShadowing.value = true
    await runWordShadowPass()
    if (!popupClosed && wantsDouble) await runWordShadowPass()
    if (!popupClosed) isShadowing.value = false
  }
}

function handleKeydown(event) {
  if (event.key === 'Escape' && visible.value) hide()
}

onMounted(() => {
  window.addEventListener('keydown', handleKeydown)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleKeydown)
})

// Shared by showWord() and a search-mode lookup - fetches and displays one
// word's info. Doesn't touch `mode` itself, so a search-mode lookup stays in
// search mode (search bar still visible) rather than becoming word mode.
async function loadWordInfo(word, { playWordOnOpen = false } = {}) {
  popupClosed = false
  loading.value = true
  error.value = null
  info.value = null
  currentWord.value = word

  // Fired immediately, independent of the definitions fetch below, so the
  // sound plays "as soon as it's looked up" rather than waiting on whatever
  // else is still loading.
  if (playWordOnOpen) playWordPronunciation(word)

  const result = await fetchWordInfo(word)

  loading.value = false
  if (!result) {
    error.value = `No definition found for "${word}".`
    return
  }
  info.value = result
}

// Word mode - opened for one specific word (a transcript click today), no
// search bar, no path into search mode from here.
async function showWord(word, options) {
  mode.value = 'word'
  visible.value = true
  await loadWordInfo(word, options)
}

// Search mode - opened with no word yet (Home menu, or the global shortcut).
// Always starts blank, even if a previous search is still showing from last
// time this same instance was opened.
function showSearch() {
  mode.value = 'search'
  visible.value = true
  popupClosed = false
  searchInput.value = ''
  currentWord.value = ''
  info.value = null
  error.value = null
  loading.value = false
}

function handleSearchSubmit() {
  const trimmed = searchInput.value.trim()
  if (!trimmed) return
  loadWordInfo(trimmed, { playWordOnOpen: true })
}

// Stops and resets any in-progress Record/Shadow session (mic recording,
// beeps, playback) immediately - closing the popup shouldn't leave any of
// that running invisibly in the background.
function hide() {
  visible.value = false
  popupClosed = true
  destroyRecordShadow()
}

defineExpose({ showWord, showSearch, hide, visible })
</script>

<style scoped>
.dictionary-backdrop {
  position: fixed;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1.5rem;
  background: rgba(0, 0, 0, 0.6);
  z-index: 100;
}

.dictionary-frame {
  position: relative;
  width: 100%;
  max-width: 24rem;
  max-height: 80vh;
  overflow-y: auto;
  padding: 1.5rem;
  background: var(--surface);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 1rem;
  text-align: left;
  color: var(--text);
}

.dictionary-close {
  position: absolute;
  top: 0.75rem;
  right: 0.75rem;
  width: 2rem;
  height: 2rem;
  border: none;
  border-radius: 50%;
  background: var(--surface-2);
  color: var(--text);
  font-size: 1.1rem;
  line-height: 1;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.dictionary-search-form {
  margin: 0 2.5rem 1rem 0;
}

.dictionary-search-input {
  width: 100%;
  padding: 0.6rem 0.9rem;
  font-size: 1rem;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.6rem;
  background: var(--surface-2);
  color: var(--text);
}

.dictionary-status {
  margin: 0;
  color: var(--text-dim);
}

.dictionary-header {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.6rem;
  margin: 0 2.5rem 0.25rem 0;
}

.dictionary-mic-error {
  margin: 0 0 0.75rem;
  max-width: none;
}

.dictionary-word {
  margin: 0;
  font-size: 1.4rem;
  font-weight: 700;
}

.dictionary-header-phonetic {
  color: var(--text-dim);
  font-size: 0.95rem;
}

.dictionary-word-play-button {
  width: 2rem;
  height: 2rem;
  font-size: 0.8rem;
}

.dictionary-word-play-button :deep(svg) {
  width: 0.85rem;
  height: 0.85rem;
}

.dictionary-phonetics {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  margin: 0 0 1rem;
}

.dictionary-phonetic {
  display: flex;
  align-items: center;
  color: var(--text-dim);
  font-size: 0.95rem;
}

.dictionary-phonetic-label {
  display: inline-block;
  margin-right: 0.4rem;
  padding: 0.05rem 0.4rem;
  border-radius: 999px;
  background: var(--surface-2);
  color: var(--accent-1);
  font-size: 0.7rem;
  font-weight: 700;
  vertical-align: middle;
}

.dictionary-phonetic-text {
  flex: 1;
}

.dictionary-play-button {
  flex-shrink: 0;
  width: 1.6rem;
  height: 1.6rem;
  margin-left: 0.5rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border: none;
  border-radius: 50%;
  background: var(--surface-2);
  color: var(--accent-1);
  font-size: 0.65rem;
  line-height: 1;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.dictionary-play-button :deep(svg) {
  width: 0.7rem;
  height: 0.7rem;
}

.dictionary-play-button:active {
  transform: scale(0.92);
}

.dictionary-meaning {
  margin-bottom: 1rem;
}

.dictionary-pos {
  margin: 0 0 0.35rem;
  font-size: 0.85rem;
  font-weight: 700;
  color: var(--accent-1);
  font-style: italic;
}

.dictionary-definitions {
  margin: 0;
  padding-left: 1.1rem;
}

.dictionary-definitions li {
  margin-bottom: 0.5rem;
}

.dictionary-definition {
  margin: 0;
}

.dictionary-example {
  margin: 0.15rem 0 0;
  color: var(--text-dim);
  font-size: 0.9rem;
  font-style: italic;
}

.dictionary-synonyms {
  margin: 0.15rem 0 0;
  color: var(--text-dim);
  font-size: 0.85rem;
}

.dictionary-attribution {
  margin: 1rem 0 0;
  color: var(--text-dim);
  font-size: 0.75rem;
}

.dictionary-attribution a {
  color: var(--accent-1);
}

.dictionary-external-links {
  display: flex;
  gap: 0.5rem;
  margin-top: 1rem;
  padding-top: 1rem;
  border-top: 1px solid rgba(255, 255, 255, 0.06);
}

.dictionary-external-link {
  padding: 0.4rem 0.9rem;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface-2);
  color: var(--accent-1);
  font-size: 0.8rem;
  font-weight: 700;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.dictionary-external-link:active {
  transform: scale(0.95);
}
</style>
