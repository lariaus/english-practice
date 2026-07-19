<template>
  <div class="word-info-backdrop" v-if="visible" @click.self="hide">
    <div class="word-info-frame">
      <button class="word-info-close" @click="hide" aria-label="Close">&times;</button>

      <p v-if="loading" class="word-info-status">Loading…</p>

      <template v-else>
        <div class="word-info-header">
          <h2 class="word-info-word">{{ currentWord }}</h2>
          <span v-if="info?.usPhonetics" class="word-info-header-phonetic">{{ info.usPhonetics.text }}</span>
          <button
            class="word-info-play-button word-info-word-play-button"
            @click="playWordPronunciation(currentWord)"
            aria-label="Play word"
          >
            ▶
          </button>
        </div>

        <template v-if="info">
          <div class="word-info-phonetics" v-if="info.phonetics.length">
            <div v-for="(p, i) in info.phonetics" :key="i" class="word-info-phonetic">
              <span class="word-info-phonetic-label">{{ p.label || '??' }}</span>
              <span class="word-info-phonetic-text">{{ p.text }}</span>
              <button
                v-if="p.audio"
                class="word-info-play-button"
                @click="playAudioUrl(p.audio)"
                aria-label="Play pronunciation"
              >
                ▶
              </button>
            </div>
          </div>

          <div class="word-info-meaning" v-for="(meaning, i) in info.meanings" :key="i">
            <h3 class="word-info-pos">{{ meaning.partOfSpeech }}</h3>
            <ol class="word-info-definitions">
              <li v-for="(def, j) in meaning.definitions.slice(0, 2)" :key="j">
                <p class="word-info-definition">{{ def.definition }}</p>
                <p class="word-info-example" v-if="def.example">"{{ def.example }}"</p>
                <p class="word-info-synonyms" v-if="def.synonyms.length">
                  syn: {{ def.synonyms.join(', ') }}
                </p>
              </li>
            </ol>
          </div>

          <p class="word-info-attribution" v-if="info.sourceUrl">
            via <a :href="info.sourceUrl" target="_blank" rel="noopener">Wiktionary</a>
            <template v-if="info.license"> · {{ info.license }}</template>
          </p>
        </template>

        <p v-else-if="error" class="word-info-status">{{ error }}</p>

        <div class="word-info-external-links">
          <button class="word-info-external-link" @click="openReferenceSite('cambridge')">Ca</button>
          <button class="word-info-external-link" @click="openReferenceSite('wordreference')">Wr</button>
        </div>
      </template>
    </div>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { fetchWordInfo } from '../engine/dictionaryClient.js'
import { playAudioUrl, playWordPronunciation } from '../engine/wordAudioPlayer.js'

const visible = ref(false)
const loading = ref(false)
const error = ref(null)
const info = ref(null)
const currentWord = ref('')

function handleKeydown(event) {
  if (event.key === 'Escape' && visible.value) hide()
}

onMounted(() => {
  window.addEventListener('keydown', handleKeydown)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleKeydown)
})

async function show(word, { playWordOnOpen = false } = {}) {
  visible.value = true
  loading.value = true
  error.value = null
  info.value = null
  currentWord.value = word

  // Fired immediately, independent of the definitions fetch below, so the
  // sound plays "as soon as the popup opens" rather than waiting on
  // whatever else is still loading.
  if (playWordOnOpen) playWordPronunciation(word)

  const result = await fetchWordInfo(word)

  loading.value = false
  if (!result) {
    error.value = `No definition found for "${word}".`
    return
  }
  info.value = result
}

function hide() {
  visible.value = false
}

const REFERENCE_SITES = {
  cambridge: (word) => `https://dictionary.cambridge.org/dictionary/english/${encodeURIComponent(word)}`,
  wordreference: (word) => `https://www.wordreference.com/definition/${encodeURIComponent(word)}`,
}

function openCenteredWindow(url) {
  const width = 700
  const height = 800
  const left = window.screenX + (window.outerWidth - width) / 2
  const top = window.screenY + (window.outerHeight - height) / 2
  window.open(url, '_blank', `width=${width},height=${height},left=${left},top=${top}`)
}

function openReferenceSite(siteId) {
  const buildUrl = REFERENCE_SITES[siteId]
  if (!buildUrl || !currentWord.value) return
  openCenteredWindow(buildUrl(currentWord.value))
}

defineExpose({ show, hide })
</script>

<style scoped>
.word-info-backdrop {
  position: fixed;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1.5rem;
  background: rgba(0, 0, 0, 0.6);
  z-index: 100;
}

.word-info-frame {
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

.word-info-close {
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

.word-info-status {
  margin: 0;
  color: var(--text-dim);
}

.word-info-header {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  margin: 0 2.5rem 0.25rem 0;
}

.word-info-word {
  margin: 0;
  font-size: 1.4rem;
  font-weight: 700;
}

.word-info-header-phonetic {
  color: var(--text-dim);
  font-size: 0.95rem;
}

.word-info-word-play-button {
  width: 2rem;
  height: 2rem;
  font-size: 0.8rem;
}

.word-info-phonetics {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  margin: 0 0 1rem;
}

.word-info-phonetic {
  display: flex;
  align-items: center;
  color: var(--text-dim);
  font-size: 0.95rem;
}

.word-info-phonetic-label {
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

.word-info-phonetic-text {
  flex: 1;
}

.word-info-play-button {
  flex-shrink: 0;
  width: 1.6rem;
  height: 1.6rem;
  margin-left: 0.5rem;
  border: none;
  border-radius: 50%;
  background: var(--surface-2);
  color: var(--accent-1);
  font-size: 0.65rem;
  line-height: 1;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.word-info-play-button:active {
  transform: scale(0.92);
}

.word-info-meaning {
  margin-bottom: 1rem;
}

.word-info-pos {
  margin: 0 0 0.35rem;
  font-size: 0.85rem;
  font-weight: 700;
  color: var(--accent-1);
  font-style: italic;
}

.word-info-definitions {
  margin: 0;
  padding-left: 1.1rem;
}

.word-info-definitions li {
  margin-bottom: 0.5rem;
}

.word-info-definition {
  margin: 0;
}

.word-info-example {
  margin: 0.15rem 0 0;
  color: var(--text-dim);
  font-size: 0.9rem;
  font-style: italic;
}

.word-info-synonyms {
  margin: 0.15rem 0 0;
  color: var(--text-dim);
  font-size: 0.85rem;
}

.word-info-attribution {
  margin: 1rem 0 0;
  color: var(--text-dim);
  font-size: 0.75rem;
}

.word-info-attribution a {
  color: var(--accent-1);
}

.word-info-external-links {
  display: flex;
  gap: 0.5rem;
  margin-top: 1rem;
  padding-top: 1rem;
  border-top: 1px solid rgba(255, 255, 255, 0.06);
}

.word-info-external-link {
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

.word-info-external-link:active {
  transform: scale(0.95);
}
</style>
