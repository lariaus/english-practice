<template>
  <main class="screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1 class="title-with-count">
      {{ setName }}
      <span v-if="isOffline" class="count-badge">{{ cards.length }} cards</span>
      <span v-else class="count-badge">{{ learnedCount }}/{{ cards.length }} learned</span>
    </h1>

    <p v-if="error" class="error-message">{{ error }}</p>

    <div class="mode-buttons">
      <button
        class="mode-button"
        :disabled="isOffline || unlearnedCount === 0"
        :title="isOffline ? 'Requires a connection' : null"
        @click="$emit('learn')"
      >
        <template v-if="isOffline">Learn Flashcards <OfflineIcon /></template>
        <template v-else>Learn ({{ learnCount }})</template>
      </button>
      <button
        class="mode-button"
        :disabled="isOffline || dueCount === 0"
        :title="isOffline ? 'Requires a connection' : null"
        @click="$emit('review')"
      >
        <template v-if="isOffline">Review Flashcards <OfflineIcon /></template>
        <template v-else>Review ({{ dueCount }})</template>
      </button>
      <button class="mode-button" @click="$emit('practice')">Practice</button>
    </div>

    <div class="top-controls">
      <input class="text-input search-input" type="text" placeholder="Search flashcards" v-model="searchQuery" />
      <button
        class="icon-button"
        aria-label="Add card"
        :disabled="isOffline"
        :title="isOffline ? 'Requires a connection' : null"
        @click="showAddForm = !showAddForm"
      >+</button>
      <button class="toggle-button" :disabled="isOffline" :title="isOffline ? 'Requires a connection' : null" @click="handleImportClick">
        Import CSV
      </button>
      <button class="toggle-button" :disabled="isOffline" :title="isOffline ? 'Requires a connection' : null" @click="handleExport">
        Export CSV
      </button>
    </div>
    <input
      ref="fileInputRef"
      type="file"
      accept=".csv,text/csv"
      class="hidden-file-input"
      @change="handleFileSelected"
    />

    <p v-if="loading" class="subtitle">Loading…</p>
    <p v-else-if="cards.length === 0 && !showAddForm" class="subtitle">No cards yet - tap + to add one.</p>

    <template v-else>
      <p v-if="!showAddForm && filteredCards.length === 0" class="subtitle">No cards match "{{ searchQuery }}".</p>

      <div v-if="showAddForm || filteredCards.length > 0" class="card-grid">
        <div v-if="showAddForm" class="flash-tile">
          <input class="text-input" v-model="newFront" placeholder="Front" />
          <input class="text-input" v-model="newBack" placeholder="Back" />
          <div class="tile-actions">
            <button class="primary-button" :disabled="!newFront.trim() || !newBack.trim()" @click="handleAdd">
              Add
            </button>
            <button class="toggle-button" @click="showAddForm = false">Cancel</button>
          </div>
        </div>

        <div v-for="card in filteredCards" :key="card.uid" class="flash-tile">
          <template v-if="editingUid === card.uid">
            <input class="text-input" v-model="editFront" placeholder="Front" />
            <input class="text-input" v-model="editBack" placeholder="Back" />
            <div class="tile-actions">
              <button class="primary-button" @click="handleSaveEdit(card.uid)">Save</button>
              <button class="toggle-button" @click="editingUid = null">Cancel</button>
            </div>
          </template>
          <template v-else>
            <p class="tile-front">{{ card.front }}</p>
            <hr class="tile-divider" />
            <p class="tile-back">{{ card.back }}</p>
            <div class="tile-actions">
              <button
                class="tile-button"
                :disabled="isOffline"
                :title="isOffline ? 'Requires a connection' : null"
                @click="handleStartEdit(card)"
              >Edit</button>
              <button
                class="tile-button tile-delete"
                :disabled="isOffline"
                :title="isOffline ? 'Requires a connection' : null"
                @click="handleDelete(card.uid)"
              >Delete</button>
            </div>
          </template>
        </div>
      </div>
    </template>

    <button
      class="toggle-button reset-set-button"
      :disabled="isOffline"
      :title="isOffline ? 'Requires a connection' : null"
      @click="handleReset"
    >Reset set</button>

    <ConfirmDialog ref="confirmDialogRef" />
  </main>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { exportCsv, resetSet } from '../engine/flashcardsClient.js'
import {
  addCardCached,
  deleteCardCached,
  editCardCached,
  getSetOrCached,
  importCsvCached,
} from '../engine/flashcardsOfflineCache.js'
import { showToast } from '../composables/useToast.js'
import { today } from '../engine/flashcardsToday.js'
import { BATCH_SIZE } from '../engine/flashcardsConstants.js'
import ConfirmDialog from '../components/ConfirmDialog.vue'
import OfflineIcon from '../components/OfflineIcon.vue'

const props = defineProps({
  setName: { type: String, required: true },
})
defineEmits(['back', 'learn', 'review', 'practice'])

const confirmDialogRef = ref(null)
const loading = ref(true)
const error = ref(null)
const cards = ref([])
const fileInputRef = ref(null)
const searchQuery = ref('')
const showAddForm = ref(false)
const isOffline = ref(false)

// A card is "learned" once it's left state NEW - see docs/flashcards-spec.md.
const learnedCount = computed(() => cards.value.filter((c) => c.state !== 'NEW').length)
const unlearnedCount = computed(() => cards.value.length - learnedCount.value)
// A Learn session only ever processes up to BATCH_SIZE cards at once (see
// docs/flashcards-spec.md's Learning section) - the button shows how many
// will actually be learned *this session*, not the full unlearned pool.
// Review has no such cap on the number shown - see the template.
const learnCount = computed(() => Math.min(BATCH_SIZE, unlearnedCount.value))
const dueCount = computed(() => {
  const todayStr = today()
  return cards.value.filter((c) => c.state === 'REVIEW' && c.due <= todayStr).length
})

// Newest-first in the grid (uids are assigned in increasing creation
// order) - cards.value itself stays oldest-first, matching the server's
// own order, since nothing else here is index-dependent.
const filteredCards = computed(() => {
  const query = searchQuery.value.trim().toLowerCase()
  const matches = query
    ? cards.value.filter((c) => c.front.toLowerCase().includes(query) || c.back.toLowerCase().includes(query))
    : cards.value
  return [...matches].reverse()
})

const newFront = ref('')
const newBack = ref('')

const editingUid = ref(null)
const editFront = ref('')
const editBack = ref('')

onMounted(async () => {
  try {
    const result = await getSetOrCached(props.setName)
    cards.value = result.set.cards
    isOffline.value = result.offline
  } catch (e) {
    error.value = e.message
    // A connectivity failure with nothing cached yet for this particular
    // set still means every mutating action here is doomed to fail -
    // disable them the same as a successful cache fallback would, even
    // though there's no cached content to actually show.
    if (e.offline) isOffline.value = true
  } finally {
    loading.value = false
  }
})

async function handleAdd() {
  const front = newFront.value.trim()
  const back = newBack.value.trim()
  if (!front || !back) return

  const isDuplicate = cards.value.some((c) => c.front.toLowerCase() === front.toLowerCase())
  if (isDuplicate) {
    const confirmed = await confirmDialogRef.value.confirm(
      `Another flashcard already has the front "${front}". Are you sure you want to add a duplicate?`,
      { confirmLabel: 'Add' },
    )
    if (!confirmed) return
  }

  try {
    const uid = await addCardCached(props.setName, { front, back })
    cards.value.push({ uid, front, back, state: 'NEW' })
    newFront.value = ''
    newBack.value = ''
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

function handleStartEdit(card) {
  editingUid.value = card.uid
  editFront.value = card.front
  editBack.value = card.back
}

async function handleSaveEdit(uid) {
  const updated = { front: editFront.value, back: editBack.value }
  try {
    await editCardCached(props.setName, uid, updated)
    const card = cards.value.find((c) => c.uid === uid)
    Object.assign(card, updated)
    editingUid.value = null
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

async function handleDelete(uid) {
  try {
    await deleteCardCached(props.setName, uid)
    cards.value = cards.value.filter((c) => c.uid !== uid)
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

// The server does all the CSV building - this just triggers a browser
// download of whatever text comes back, see docs/flashcards-spec.md's
// "CSV import/export" section.
async function handleExport() {
  try {
    const csvText = await exportCsv(props.setName)
    const blob = new Blob([csvText], { type: 'text/csv;charset=utf-8' })
    const url = URL.createObjectURL(blob)
    const link = document.createElement('a')
    link.href = url
    link.download = `${props.setName}.csv`
    link.click()
    URL.revokeObjectURL(url)
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

function handleImportClick() {
  fileInputRef.value?.click()
}

// No client-side CSV parsing at all - the raw file text is sent as-is, the
// server does all the parsing/validation.
async function handleFileSelected(event) {
  const file = event.target.files?.[0]
  event.target.value = '' // reset so picking the same file again still fires @change
  if (!file) return

  try {
    const csvText = await file.text()
    const set = await importCsvCached(props.setName, csvText)
    cards.value = set.cards
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

async function handleReset() {
  const confirmed = await confirmDialogRef.value.confirm(
    `Reset "${props.setName}"? This wipes all learning/review progress - the cards themselves stay. Can't be undone.`,
    { confirmLabel: 'Reset' },
  )
  if (!confirmed) return
  try {
    const set = await resetSet(props.setName)
    cards.value = set.cards
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}
</script>

<style scoped>
.mode-buttons {
  display: flex;
  gap: 0.5rem;
  width: 100%;
  max-width: 64rem;
}

.mode-button {
  flex: 1;
  padding: 0.7rem 1rem;
  font-size: 0.95rem;
  font-weight: 600;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: transform 0.15s ease, background 0.15s ease;
}

.mode-button:active {
  background: var(--surface-2);
  transform: scale(0.97);
}

.mode-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  transform: none;
}

.title-with-count {
  display: flex;
  align-items: center;
  gap: 0.6rem;
}

.count-badge {
  font-size: 0.85rem;
  font-weight: 700;
  padding: 0.15rem 0.55rem;
  border-radius: 999px;
  background: var(--surface-2);
  color: var(--text-dim);
}

.top-controls {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  width: 100%;
  max-width: 64rem;
  flex-wrap: wrap;
}

.search-input {
  flex: 1;
  min-width: 10rem;
}

.icon-button {
  flex-shrink: 0;
  width: 2.5rem;
  height: 2.5rem;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  font-size: 1.4rem;
  line-height: 1;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--accent-gradient);
  color: #fff;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.icon-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.icon-button:active {
  transform: scale(0.94);
}

.top-controls .toggle-button {
  flex-shrink: 0;
  white-space: nowrap;
}

.hidden-file-input {
  display: none;
}

.card-grid {
  width: 100%;
  max-width: 64rem;
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(15rem, 1fr));
  gap: 1rem;
}

.reset-set-button {
  margin-top: 2rem;
  color: #e5484d;
  border-color: rgba(229, 72, 77, 0.3);
}
</style>
