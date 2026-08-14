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

    <div class="lists-row">
      <div class="history-section" v-if="history.length > 0">
        <div class="list-frame">
          <h2 class="history-title">History</h2>
          <div class="list-frame-scroll">
            <ul class="list-frame-list">
              <li v-for="entry in history" :key="entry.videoId">
                <button class="history-item" @click="handleHistoryClick(entry)">
                  {{ formatVideoLine(entry) }}
                </button>
              </li>
            </ul>
          </div>
        </div>
      </div>

      <div class="playlists-section">
        <!-- Same frame (border/radius, title+content+button all inside)
             as History's - always rendered, so it matches History's frame
             whether it's showing the empty state, the playlist list, or a
             drilled-in playlist's videos. -->
        <div class="list-frame">
          <div class="list-frame-header">
            <button v-if="openPlaylistName" class="playlist-back-link" @click="handleClosePlaylist">
              &larr; Back to playlists
            </button>
            <h2 v-else class="history-title">Playlists</h2>
          </div>

          <div class="list-frame-scroll">
            <template v-if="openPlaylistName">
              <p v-if="openPlaylistLoading" class="subtitle">Loading…</p>
              <p v-else-if="openPlaylistError" class="error-message">{{ openPlaylistError }}</p>
              <p v-else-if="openPlaylistVideos.length === 0" class="subtitle">No videos in this playlist yet.</p>
              <ul v-else class="list-frame-list">
                <li v-for="video in openPlaylistVideos" :key="video.videoId" class="playlist-row">
                  <button class="history-item" @click="handlePlaylistVideoClick(video)">
                    {{ formatPlaylistVideoLine(video) }}
                  </button>
                  <button
                    class="delete-button"
                    aria-label="Remove from playlist"
                    @click="handleRemoveVideo(video.videoId)"
                  >×</button>
                </li>
              </ul>
            </template>

            <template v-else>
              <p v-if="playlists.length === 0" class="subtitle">No playlists yet - create one below.</p>

              <ul v-else class="list-frame-list">
                <li v-for="playlist in playlists" :key="playlist.name" class="playlist-row">
                  <template v-if="editingPlaylistName === playlist.name">
                    <input
                      class="text-input playlist-rename-input"
                      v-model="editPlaylistNewName"
                      @keyup.enter="handleSaveRename(playlist.name)"
                    />
                    <button class="tile-button" @click="handleSaveRename(playlist.name)">Save</button>
                    <button class="toggle-button" @click="handleCancelRename">Cancel</button>
                  </template>
                  <template v-else>
                    <button class="history-item" @click="handleOpenPlaylist(playlist.name)">
                      {{ playlist.name }}
                    </button>
                    <button
                      class="tile-button"
                      aria-label="Rename playlist"
                      @click="handleStartRename(playlist.name)"
                    >&#9998;</button>
                    <button
                      class="delete-button"
                      aria-label="Delete playlist"
                      @click="handleDeletePlaylist(playlist.name)"
                    >×</button>
                  </template>
                </li>
              </ul>

              <div v-if="showCreateForm" class="flash-tile create-playlist-form">
                <input
                  class="text-input"
                  type="text"
                  placeholder="New playlist name"
                  v-model="newPlaylistName"
                  @keyup.enter="handleCreatePlaylist"
                />
                <div class="tile-actions">
                  <button
                    class="primary-button"
                    :disabled="!newPlaylistName.trim() || creatingPlaylist"
                    @click="handleCreatePlaylist"
                  >Create</button>
                  <button class="toggle-button" @click="handleCancelCreatePlaylist">Cancel</button>
                </div>
              </div>
              <button v-else class="toggle-button create-playlist-button" @click="showCreateForm = true">
                Create playlist
              </button>
            </template>
          </div>
        </div>
      </div>
    </div>

    <ConfirmDialog ref="confirmDialogRef" />
  </main>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { parseYouTubeVideoId } from '../engine/ytShadowingEngine.js'
import { loadHistory } from '../engine/ytHistory.js'
import {
  createPlaylist,
  deletePlaylist,
  getPlaylist,
  listPlaylists,
  removeVideoFromPlaylist,
  renamePlaylist,
} from '../engine/ytPlaylistsClient.js'
import { formatVideoLine } from '../engine/ytVideoFormat.js'
import { showToast } from '../composables/useToast.js'
import ConfirmDialog from '../components/ConfirmDialog.vue'

const emit = defineEmits(['back', 'load'])

const videoUrl = ref('')
const error = ref(null)
const history = ref([])
const historyByVideoId = computed(() => new Map(history.value.map((entry) => [entry.videoId, entry])))

const confirmDialogRef = ref(null)
const playlists = ref([])

const showCreateForm = ref(false)
const newPlaylistName = ref('')
const creatingPlaylist = ref(false)

// Only one row's rename input is ever open at once - opening another row's
// pencil while mid-edit silently discards the unsaved one, same as
// FlashcardsEditScreen.vue's editingUid.
const editingPlaylistName = ref(null)
const editPlaylistNewName = ref('')

const openPlaylistName = ref(null)
const openPlaylistVideos = ref([])
const openPlaylistError = ref(null)
const openPlaylistLoading = ref(false)

onMounted(async () => {
  history.value = await loadHistory()
  playlists.value = await listPlaylists()
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

// Progress for a playlist video always comes from History when the video's
// also there (History's duration/currentPosition are always kept fresh; a
// playlist's own copy of duration is frozen at add-time) - falls back to
// the playlist's own stored duration, with no progress segment, otherwise.
// See docs/yt-shadowing-spec.md's "Playlists" section.
function formatPlaylistVideoLine(video) {
  const historyMatch = historyByVideoId.value.get(video.videoId)
  return formatVideoLine({
    title: video.title,
    author: video.author,
    duration: historyMatch?.duration ?? video.duration,
    currentPosition: historyMatch?.currentPosition ?? 0,
  })
}

async function refreshPlaylists() {
  playlists.value = await listPlaylists()
}

async function handleCreatePlaylist() {
  const name = newPlaylistName.value.trim()
  if (!name) return
  creatingPlaylist.value = true
  try {
    await createPlaylist(name)
    newPlaylistName.value = ''
    showCreateForm.value = false
    await refreshPlaylists()
  } catch (e) {
    showToast(e.message, { type: 'error' })
  } finally {
    creatingPlaylist.value = false
  }
}

function handleCancelCreatePlaylist() {
  newPlaylistName.value = ''
  showCreateForm.value = false
}

function handleStartRename(name) {
  editingPlaylistName.value = name
  editPlaylistNewName.value = name
}

function handleCancelRename() {
  editingPlaylistName.value = null
}

async function handleSaveRename(name) {
  const newName = editPlaylistNewName.value.trim()
  if (!newName) return
  try {
    await renamePlaylist(name, newName)
    editingPlaylistName.value = null
    await refreshPlaylists()
    // Keep the drill-in view (if this exact playlist is the one open) in
    // sync with its new name, rather than pointing at a name that no
    // longer exists.
    if (openPlaylistName.value === name) openPlaylistName.value = newName
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

async function handleDeletePlaylist(name) {
  const confirmed = await confirmDialogRef.value.confirm(`Delete playlist "${name}"? This can't be undone.`, {
    confirmLabel: 'Delete',
  })
  if (!confirmed) return
  try {
    await deletePlaylist(name)
    await refreshPlaylists()
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

async function handleOpenPlaylist(name) {
  openPlaylistName.value = name
  openPlaylistError.value = null
  openPlaylistLoading.value = true
  try {
    const playlist = await getPlaylist(name)
    openPlaylistVideos.value = playlist.videos
  } catch (e) {
    // A real error state here, not a silent empty list - see
    // ytPlaylistsClient.js's header comment for why this one call throws
    // instead of degrading quietly like listPlaylists() does.
    openPlaylistError.value = e.message
  } finally {
    openPlaylistLoading.value = false
  }
}

function handleClosePlaylist() {
  openPlaylistName.value = null
  openPlaylistVideos.value = []
  openPlaylistError.value = null
}

function handlePlaylistVideoClick(video) {
  emit('load', { videoId: video.videoId, url: video.url })
}

async function handleRemoveVideo(videoId) {
  try {
    await removeVideoFromPlaylist(openPlaylistName.value, videoId)
    openPlaylistVideos.value = openPlaylistVideos.value.filter((v) => v.videoId !== videoId)
    // This playlist's updatedAt just changed - refresh the top-level list's
    // sort order for whenever the user goes back to it.
    await refreshPlaylists()
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
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

/* .screen's own layout (style.css) centers a single column - this wraps
   History and Playlists side by side instead, wrapping to stacked on
   narrow screens rather than squeezing both into half the width. */
.lists-row {
  display: flex;
  flex-wrap: wrap;
  gap: 1.5rem;
  justify-content: center;
  width: 100%;
  max-width: 44rem;
}

.history-section,
.playlists-section {
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

/* The single bordered frame both History and Playlists sit in - title,
   scrollable content, and (Playlists only) the create button/form all
   live inside it, so the two panels' frames always match in width/style
   regardless of which one has more content. */
.list-frame {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  padding: 0.75rem;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.75rem;
}

.list-frame-header {
  display: flex;
  align-items: center;
}

/* Only the list itself scrolls - the title/back-link above and the create
   button/form below (Playlists) stay put rather than scrolling away. */
/* A fixed height (not max-height) so History and Playlists always match
   vertically regardless of how much content each has - short content just
   leaves empty space in the box instead of shrinking it, and longer
   content scrolls within it. */
.list-frame-scroll {
  height: min(24rem, 50vh);
  overflow-y: auto;
}

.list-frame-list {
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

.playlist-row {
  display: flex;
  gap: 0.4rem;
  align-items: stretch;
}

.playlist-row .history-item {
  flex: 1;
}

.playlist-row .tile-button,
.playlist-row .delete-button {
  flex-shrink: 0;
  width: auto;
  padding: 0 0.9rem;
}

.playlist-rename-input {
  flex: 1;
}

.playlist-back-link {
  align-self: flex-start;
  padding: 0;
  border: none;
  background: none;
  color: var(--text-dim);
  font-size: 0.85rem;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.create-playlist-button,
.create-playlist-form {
  width: 100%;
  /* Sits right below the last playlist row, inside .list-frame-scroll,
     rather than pinned below the scrollable area - that scroll container
     is a plain block box (not flex), so this needs its own top margin
     instead of relying on a flex gap from a sibling. */
  margin-top: 0.5rem;
}
</style>
