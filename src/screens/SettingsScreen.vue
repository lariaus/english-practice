<template>
  <main class="screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1>Settings</h1>

    <div class="settings-field">
      <label class="settings-label" for="sync-server-url">Sync server URL</label>
      <input
        id="sync-server-url"
        class="settings-input"
        type="text"
        inputmode="url"
        placeholder="https://your-worker.workers.dev"
        v-model="serverUrl"
      />
      <p class="settings-hint">
        Leave empty to disable cross-device sync. Never stored in the app's
        source - only on this device.
      </p>
    </div>

    <div class="settings-buttons">
      <button class="primary-button" @click="handleSave">
        {{ saved ? 'Saved' : 'Save' }}
      </button>
      <button class="test-button" :disabled="!serverUrl.trim()" @click="handleTest">
        {{ testStatus === 'testing' ? 'Testing…' : 'Test' }}
      </button>
    </div>

    <p v-if="testStatus === 'success'" class="test-message test-success">✓ Connected</p>
    <p v-if="testStatus === 'error'" class="test-message test-error">✗ Could not reach server</p>

    <button class="logs-link" @click="$emit('open-logs')">Logs</button>
  </main>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { getSyncServerUrl, setSyncServerUrl } from '../engine/syncConfig.js'

defineEmits(['back', 'open-logs'])

const serverUrl = ref('')
const saved = ref(false)
const testStatus = ref(null) // null | 'testing' | 'success' | 'error'

onMounted(async () => {
  serverUrl.value = await getSyncServerUrl()
})

async function handleSave() {
  await setSyncServerUrl(serverUrl.value)
  serverUrl.value = await getSyncServerUrl()
  saved.value = true
  setTimeout(() => {
    saved.value = false
  }, 1500)
}

// Tests whatever's currently typed, not necessarily what's saved yet - hits
// GET /history (the Worker has no separate health endpoint) and just checks
// the response comes back ok.
async function handleTest() {
  testStatus.value = 'testing'
  const url = serverUrl.value.trim().replace(/\/+$/, '')
  if (!url) {
    testStatus.value = 'error'
    return
  }

  try {
    const response = await fetch(`${url}/history`)
    testStatus.value = response.ok ? 'success' : 'error'
  } catch {
    testStatus.value = 'error'
  }
}
</script>

<style scoped>
.settings-field {
  width: 100%;
  max-width: 20rem;
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
  text-align: left;
}

.settings-label {
  font-size: 0.85rem;
  font-weight: 600;
  color: var(--text-dim);
}

.settings-input {
  width: 100%;
  padding: 0.8rem 1rem;
  font-size: 1rem;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 0.6rem;
  background: var(--surface);
  color: var(--text);
}

.settings-hint {
  margin: 0;
  font-size: 0.75rem;
  color: var(--text-dim);
}

.settings-buttons {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.test-button {
  padding: 1rem 1.4rem;
  font-size: 1.1rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 999px;
  background: none;
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: background 0.15s ease, transform 0.15s ease;
}

.test-button:active {
  transform: scale(0.97);
  background: var(--surface-2);
}

.test-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.test-message {
  margin: 0;
  font-size: 0.9rem;
  font-weight: 600;
}

.test-success {
  color: #4ade80;
}

.test-error {
  color: var(--record);
}

.logs-link {
  padding: 0;
  border: none;
  background: none;
  color: var(--text-dim);
  font-size: 0.85rem;
  text-decoration: underline;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}
</style>
