<template>
  <main class="screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1>Flashcards</h1>

    <p v-if="error" class="error-message">{{ error }}</p>

    <p v-if="loading" class="subtitle">Loading…</p>
    <p v-else-if="sets.length === 0" class="subtitle">No sets yet - create one below.</p>

    <ul v-else class="tool-list set-list">
      <li v-for="name in sets" :key="name" class="set-row">
        <button class="tool-button" @click="$emit('select-set', name)">
          <span class="tool-icon">
            <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
              <rect x="3" y="6" width="15" height="11" rx="2" stroke="white" stroke-width="2" />
              <rect x="6" y="3" width="15" height="11" rx="2" stroke="white" stroke-width="2" opacity="0.5" />
            </svg>
          </span>
          <span class="tool-label">{{ name }}</span>
          <span class="tool-chevron">›</span>
        </button>
        <button
          class="delete-button"
          aria-label="Delete set"
          :disabled="isOffline"
          :title="isOffline ? 'Requires a connection' : null"
          @click="handleDelete(name)"
        >×</button>
      </li>
    </ul>

    <div v-if="showCreateForm" class="flash-tile create-set-form">
      <input
        class="text-input"
        type="text"
        placeholder="New set name"
        v-model="newSetName"
        @keyup.enter="handleCreate"
      />
      <div class="tile-actions">
        <button class="primary-button" :disabled="!newSetName.trim() || creating" @click="handleCreate">
          Create
        </button>
        <button class="toggle-button" @click="handleCancelCreate">Cancel</button>
      </div>
    </div>
    <button
      v-else
      class="toggle-button create-set-button"
      :disabled="isOffline"
      :title="isOffline ? 'Requires a connection' : null"
      @click="showCreateForm = true"
    >Create new set</button>

    <ConfirmDialog ref="confirmDialogRef" />
  </main>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { createSetCached, deleteSetCached, listSetsOrCached } from '../engine/flashcardsOfflineCache.js'
import { showToast } from '../composables/useToast.js'
import ConfirmDialog from '../components/ConfirmDialog.vue'

defineEmits(['back', 'select-set'])

const confirmDialogRef = ref(null)
const sets = ref([])
const loading = ref(true)
const creating = ref(false)
const newSetName = ref('')
const showCreateForm = ref(false)
const error = ref(null)
const isOffline = ref(false)

async function load() {
  loading.value = true
  try {
    const result = await listSetsOrCached()
    sets.value = result.names
    isOffline.value = result.offline
    error.value = null
  } catch (e) {
    error.value = e.message
    // See FlashcardsEditScreen.vue's onMounted for why this still matters
    // even with nothing cached to show.
    if (e.offline) isOffline.value = true
  } finally {
    loading.value = false
  }
}

onMounted(load)

async function handleCreate() {
  const name = newSetName.value.trim()
  if (!name) return
  creating.value = true
  try {
    await createSetCached(name)
    newSetName.value = ''
    showCreateForm.value = false
    await load()
  } catch (e) {
    showToast(e.message, { type: 'error' })
  } finally {
    creating.value = false
  }
}

function handleCancelCreate() {
  newSetName.value = ''
  showCreateForm.value = false
}

async function handleDelete(name) {
  const confirmed = await confirmDialogRef.value.confirm(`Delete "${name}"? This can't be undone.`, {
    confirmLabel: 'Delete',
  })
  if (!confirmed) return
  try {
    await deleteSetCached(name)
    await load()
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}
</script>

<style scoped>
.create-set-button {
  width: 100%;
  max-width: 24rem;
}

.create-set-form {
  width: 100%;
  max-width: 24rem;
}

.set-list {
  width: 100%;
  max-width: 24rem;
}

.set-row {
  display: flex;
  gap: 0.5rem;
  align-items: stretch;
}

.set-row .tool-button {
  flex: 1;
}

.set-row .delete-button {
  flex-shrink: 0;
  width: auto;
  padding: 0 1rem;
  font-size: 1.4rem;
}
</style>
