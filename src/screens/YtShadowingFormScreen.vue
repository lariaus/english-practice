<template>
  <main class="screen yt-form-screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1>YT Shadowing</h1>

    <p v-if="error" class="error-message">{{ error }}</p>

    <input
      class="yt-url-input"
      type="text"
      inputmode="url"
      placeholder="Paste a YouTube video URL"
      v-model="videoUrl"
    />

    <button class="primary-button" :disabled="!videoUrl" @click="handleLoad">
      Load
    </button>

    <div class="history-section" v-if="history.length > 0">
      <h2 class="history-title">History</h2>
      <ul class="history-list">
        <li v-for="entry in history" :key="entry.videoId">
          <button class="history-item" @click="handleHistoryClick(entry)">
            {{ formatHistoryLine(entry) }}
          </button>
        </li>
      </ul>
    </div>
  </main>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { parseYouTubeVideoId } from '../engine/ytShadowingEngine.js'
import { loadHistory } from '../engine/ytHistory.js'

const emit = defineEmits(['back', 'load'])

const videoUrl = ref('')
const error = ref(null)
const history = ref([])

onMounted(async () => {
  history.value = await loadHistory()
})

function handleLoad() {
  const videoId = parseYouTubeVideoId(videoUrl.value)
  if (!videoId) {
    error.value = 'Could not find a video ID in that URL.'
    return
  }

  emit('load', { videoId, url: videoUrl.value })
}

function handleHistoryClick(entry) {
  emit('load', { videoId: entry.videoId, url: entry.url })
}

function formatTime(totalSeconds) {
  const safeSeconds = Number.isFinite(totalSeconds) ? Math.max(0, totalSeconds) : 0
  const minutes = Math.floor(safeSeconds / 60)
  const seconds = Math.floor(safeSeconds % 60)
  return `${minutes}:${String(seconds).padStart(2, '0')}`
}

const TITLE_MAX_LENGTH = 50

// Truncated explicitly in JS (not left to CSS's single-line ellipsis) so a
// long title can never crowd the author/duration/progress that follows it
// off the edge of the box - those would otherwise just get silently cut off
// along with the rest of an overflowing line.
function truncateTitle(title) {
  if (title.length <= TITLE_MAX_LENGTH) return title
  return `${title.slice(0, TITLE_MAX_LENGTH)}...`
}

// "Title - Author (duration) (progress%)" - each trailing part only appears
// if that data exists (author/duration missing on older entries written
// before those fields existed), and progress only shows once it's at least
// 1% (a fresh, barely-started entry would otherwise show a noisy "(0%)").
function formatHistoryLine(entry) {
  let line = truncateTitle(entry.title)
  if (entry.author) line += ` - ${entry.author}`
  if (entry.duration) line += ` (${formatTime(entry.duration)})`
  if (entry.duration > 0) {
    const progressPercent = Math.round(((entry.currentPosition || 0) / entry.duration) * 100)
    if (progressPercent >= 1) line += ` (${progressPercent}%)`
  }
  return line
}
</script>

<style scoped>
.yt-url-input {
  width: 100%;
  max-width: 20rem;
  padding: 0.8rem 1rem;
  font-size: 1rem;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.6rem;
  background: var(--surface);
  color: var(--text);
}

.history-section {
  width: 100%;
  max-width: 20rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.history-title {
  margin: 0;
  font-size: 0.9rem;
  font-weight: 600;
  color: var(--text-dim);
  text-align: left;
}

.history-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.history-item {
  width: 100%;
  padding: 0.7rem 0.9rem;
  font-size: 0.9rem;
  line-height: 1.4;
  font-variant-numeric: tabular-nums;
  text-align: left;
  white-space: normal;
  word-break: break-word;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.6rem;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease;
}

.history-item:active {
  background: var(--surface-2);
}
</style>
