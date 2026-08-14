<!-- Add the given video to an existing or brand-new playlist - see
     docs/yt-shadowing-spec.md's "Playlists" section. Locally owned by
     whichever screen opens it (YtShadowingPlayerScreen.vue), not a global
     singleton like DictionaryPopup.vue - it's only ever triggered from one
     place, see docs/common-design-philosophy.md's "Modal popups" section
     for when that's the right call. Same backdrop-plus-centered-card shape
     as ConfirmDialog.vue, sized for a scrollable list rather than a single
     message. -->
<template>
  <div class="playlist-popup-backdrop" @click.self="handleClose">
    <div class="playlist-popup" role="dialog" aria-modal="true">
      <div class="playlist-popup-header">
        <h2 class="playlist-popup-title">Add to Playlist</h2>
        <button class="popup-close-button" aria-label="Close" @click="handleClose">&times;</button>
      </div>

      <p v-if="loading" class="subtitle">Loading…</p>
      <p v-else-if="playlists.length === 0 && !showCreateForm" class="subtitle">
        No playlists yet - create one below.
      </p>

      <ul v-else class="playlist-popup-list">
        <li v-for="playlist in playlists" :key="playlist.name">
          <button class="playlist-popup-item" :disabled="adding" @click="handleAddToExisting(playlist.name)">
            {{ playlist.name }}
          </button>
        </li>
      </ul>

      <div v-if="showCreateForm" class="flash-tile create-playlist-form">
        <input
          class="text-input"
          type="text"
          placeholder="New playlist name"
          v-model="newPlaylistName"
          @keyup.enter="handleCreate"
        />
        <div class="tile-actions">
          <button class="primary-button" :disabled="!newPlaylistName.trim() || adding" @click="handleCreate">
            Create
          </button>
          <button class="toggle-button" @click="handleCancelCreate">Cancel</button>
        </div>
      </div>
      <button v-else class="toggle-button create-playlist-button" :disabled="adding" @click="showCreateForm = true">
        Create new playlist
      </button>
    </div>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { addVideoToPlaylist, createPlaylist, listPlaylists } from '../engine/ytPlaylistsClient.js'
import { showToast } from '../composables/useToast.js'

const props = defineProps({
  video: { type: Object, required: true }, // {videoId, url, title, author, duration}
})
const emit = defineEmits(['close'])

const playlists = ref([])
const loading = ref(true)
const showCreateForm = ref(false)
const newPlaylistName = ref('')
const adding = ref(false)

onMounted(async () => {
  playlists.value = await listPlaylists()
  loading.value = false
  window.addEventListener('keydown', handleKeydown)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleKeydown)
})

function handleKeydown(event) {
  if (event.key === 'Escape') handleClose()
}

function handleClose() {
  emit('close')
}

// Shows a toast either way - "already in X" isn't an error, just a no-op
// worth telling the user about (see docs/yt-shadowing-spec.md).
function reportAddResult(name, result) {
  showToast(result.added ? `Added to "${name}"` : `Video already in "${name}"`)
}

async function handleAddToExisting(name) {
  adding.value = true
  try {
    const result = await addVideoToPlaylist(name, props.video)
    reportAddResult(name, result)
    emit('close')
  } catch (e) {
    showToast(e.message, { type: 'error' })
  } finally {
    adding.value = false
  }
}

async function handleCreate() {
  const name = newPlaylistName.value.trim()
  if (!name) return
  adding.value = true
  try {
    await createPlaylist(name)
    const result = await addVideoToPlaylist(name, props.video)
    reportAddResult(name, result)
    emit('close')
  } catch (e) {
    showToast(e.message, { type: 'error' })
  } finally {
    adding.value = false
  }
}

function handleCancelCreate() {
  newPlaylistName.value = ''
  showCreateForm.value = false
}
</script>

<style scoped>
.playlist-popup-backdrop {
  position: fixed;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1.5rem;
  background: rgba(0, 0, 0, 0.5);
  backdrop-filter: blur(6px);
  -webkit-backdrop-filter: blur(6px);
  z-index: 200;
}

.playlist-popup {
  width: 100%;
  max-width: 22rem;
  display: flex;
  flex-direction: column;
  gap: 1rem;
  padding: 1.5rem;
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 1rem;
  background: var(--surface);
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
}

.playlist-popup-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
}

.playlist-popup-title {
  margin: 0;
  font-size: 1.1rem;
}

.popup-close-button {
  flex-shrink: 0;
  width: 2rem;
  height: 2rem;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  font-size: 1.4rem;
  line-height: 1;
  border: none;
  border-radius: 50%;
  background: none;
  color: var(--text-dim);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.playlist-popup-list {
  list-style: none;
  margin: 0;
  padding: 0.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  max-height: min(18rem, 40vh);
  overflow-y: auto;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.75rem;
}

.playlist-popup-item {
  width: 100%;
  padding: 0.7rem 0.9rem;
  font-size: 0.9rem;
  text-align: left;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.6rem;
  background: var(--surface-2);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease;
}

.playlist-popup-item:active {
  background: rgba(255, 255, 255, 0.1);
}

.playlist-popup-item:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.create-playlist-button,
.create-playlist-form {
  width: 100%;
}
</style>
