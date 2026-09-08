<template>
  <main class="screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1>Data Packs</h1>

    <p class="packs-hint">
      Pulls a pre-crawled dictionary pack from another device's
      <code>shared_data</code> (over the local network) into this device's own
      live <code>server_data</code>. Only a device running native-server as
      the CLI ever has real packs to serve - point this at that machine's
      address, even if it's this same machine.
    </p>

    <div class="address-field">
      <label for="remote-address">Server address</label>
      <input
        id="remote-address"
        v-model="remoteAddress"
        type="text"
        placeholder="192.168.1.5:8000"
        @blur="handleLoadPacks"
        @keydown.enter="handleLoadPacks"
      />
    </div>

    <p v-if="loaded && packs.length === 0" class="packs-empty">
      No packs found at that address.
    </p>

    <div v-else-if="loaded" class="packs-list">
      <div v-for="pack in packs" :key="pack" class="pack-row">
        <div class="pack-name">{{ pack }}</div>

        <div v-if="packResults[pack]" class="pack-progress">
          <progress :max="packResults[pack].total || 1" :value="packResults[pack].copied"></progress>
          <span class="pack-status" :class="packStatusClass(pack)">{{ packStatusLabel(pack) }}</span>
        </div>

        <button class="sync-button" :disabled="anyOperationRunning" @click="handleSync(pack)">
          Sync
        </button>
      </div>
    </div>

    <div class="server-data-footer">
      <span class="server-data-size">server_data: {{ formattedServerDataSize }}</span>
      <button class="clear-button" :disabled="anyOperationRunning" @click="handleClear">
        {{ clearing ? 'Clearing…' : 'Clear server_data' }}
      </button>
    </div>
  </main>
</template>

<script setup>
import { computed, onMounted, reactive, ref } from 'vue'
import {
  clearServerData,
  getServerDataSize,
  getSyncStatus,
  listPacks,
  startSync,
} from '../engine/dataPacksClient.js'
import { showToast } from '../composables/useToast.js'
import { StorageMap } from '../engine/storageMap.js'

defineEmits(['back'])

const POLL_INTERVAL_MS = 250
const SIZE_UNITS = ['B', 'KB', 'MB', 'GB']

// Remembered client-side only, purely a convenience so the address isn't
// retyped every visit - not part of the server's contract, which always
// takes the address per-request (see docs/data-pack-sync.md).
const ADDRESS_MAP_ID = 'core'
const ADDRESS_KEY = 'data-packs-remote-address'

const remoteAddress = ref('')
const packs = ref([])
const loaded = ref(false)
const syncing = ref(false)
const clearing = ref(false)
const serverDataBytes = ref(null)

// Refreshed only on mount and right after a sync/clear finishes - never
// polled continuously (see docs/data-pack-sync.md).
const formattedServerDataSize = computed(() => formatBytes(serverDataBytes.value))

function formatBytes(bytes) {
  if (bytes === null) return '…'
  if (bytes === 0) return '0 B'
  const exponent = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), SIZE_UNITS.length - 1)
  const value = bytes / 1024 ** exponent
  return `${exponent === 0 ? value : value.toFixed(1)} ${SIZE_UNITS[exponent]}`
}

async function refreshServerDataSize() {
  serverDataBytes.value = await getServerDataSize()
}

// Keyed by pack name - { total, copied, done, error }. Left in place after
// a sync finishes so the row keeps showing its final result persistently,
// not just a transient toast (see docs/data-pack-sync.md).
const packResults = reactive({})

// Mirrors the server's own mutual-exclusion (one sync-or-clear at a time) -
// disabling every button client-side too, not just relying on the 409 the
// server would otherwise return.
const anyOperationRunning = computed(() => syncing.value || clearing.value)

function packStatusClass(pack) {
  const result = packResults[pack]
  if (!result || !result.done) return ''
  return result.error ? 'pack-status-error' : 'pack-status-done'
}

function packStatusLabel(pack) {
  const result = packResults[pack]
  if (!result) return ''
  if (!result.done) {
    const pct = result.total > 0 ? Math.round((result.copied / result.total) * 100) : 0
    return `${result.copied} / ${result.total} files (${pct}%)`
  }
  return result.error ? `✗ ${result.error}` : `✓ ${result.copied} / ${result.total} files synced`
}

async function poll(pack) {
  const status = await getSyncStatus()
  // A transient fetch failure (status === null) or a stale response for a
  // different pack than the one we're tracking (another tab/session started
  // a different sync) - either way, just try again shortly rather than
  // tearing down the poll over one bad tick.
  if (!status || status.pack !== pack) {
    setTimeout(() => poll(pack), POLL_INTERVAL_MS)
    return
  }

  packResults[pack] = {
    total: status.total,
    copied: status.copied,
    done: status.done,
    error: status.error,
  }

  if (!status.done) {
    setTimeout(() => poll(pack), POLL_INTERVAL_MS)
    return
  }

  syncing.value = false
  if (status.error) {
    showToast(`✗ ${pack}: ${status.error}`, { type: 'error' })
  } else {
    showToast(`✓ ${pack} synced (${status.copied} files)`)
  }
  refreshServerDataSize()
}

// The server address is host:port (e.g. "192.168.1.5:8000") - the route
// layer builds "http://" + this itself. Strips a pasted-in "http://"/
// "https://" scheme and any trailing slash so pasting a URL copied from a
// browser's address bar (e.g. "http://localhost:8000/") still works,
// rather than silently building a malformed "http://http://..." request.
function normalizeAddress(value) {
  return value.trim().replace(/^https?:\/\//i, '').replace(/\/+$/, '')
}

// Called on blur/Enter from the address field - both loads that address's
// pack list and remembers it for next time. Also what a Sync click's
// remote param comes from, so there's never a way to sync against a
// different address than the one the currently-displayed list came from.
async function handleLoadPacks() {
  const normalized = normalizeAddress(remoteAddress.value)
  remoteAddress.value = normalized
  if (!normalized) {
    packs.value = []
    loaded.value = false
    return
  }

  await StorageMap.get(ADDRESS_MAP_ID).set(ADDRESS_KEY, normalized)
  const result = await listPacks(normalized)
  if (!result.ok) {
    showToast(`✗ ${result.error}`, { type: 'error' })
    packs.value = []
    loaded.value = false
    return
  }
  packs.value = result.packs
  loaded.value = true
}

async function handleSync(pack) {
  syncing.value = true
  packResults[pack] = { total: 0, copied: 0, done: false, error: null }

  const result = await startSync(pack, remoteAddress.value)
  if (!result.ok) {
    showToast(`✗ ${result.error}`, { type: 'error' })
    delete packResults[pack]
    syncing.value = false
    return
  }

  poll(pack)
}

async function handleClear() {
  // eslint-disable-next-line no-alert -- deliberate: a plain confirm() is
  // the agreed-on safeguard for this destructive action, see
  // docs/data-pack-sync.md.
  if (!confirm('Clear all synced data from server_data? This cannot be undone.')) return

  clearing.value = true
  const result = await clearServerData()
  clearing.value = false

  if (!result.ok) {
    showToast(`✗ ${result.error}`, { type: 'error' })
  } else {
    showToast('✓ server_data cleared')
  }
  refreshServerDataSize()
}

onMounted(async () => {
  remoteAddress.value = (await StorageMap.get(ADDRESS_MAP_ID).get(ADDRESS_KEY)) || ''
  if (remoteAddress.value) {
    await handleLoadPacks()
  }
  refreshServerDataSize()

  // Restores an in-progress sync's live state after a reload/re-navigation,
  // not just a static snapshot - resumes the same poll loop startSync
  // itself would have kicked off. sync-status is always about this
  // device's own last/current sync regardless of remote address, so this
  // doesn't depend on the address field above at all (see
  // docs/data-pack-sync.md).
  const status = await getSyncStatus()
  if (status && !status.done && status.pack) {
    syncing.value = true
    packResults[status.pack] = {
      total: status.total,
      copied: status.copied,
      done: status.done,
      error: status.error,
    }
    poll(status.pack)
  }
})
</script>

<style scoped>
.packs-hint {
  max-width: 26rem;
  font-size: 0.8rem;
  color: var(--text-dim);
  text-align: left;
}

.packs-hint code {
  font-family: ui-monospace, SFMono-Regular, monospace;
}

.address-field {
  width: 100%;
  max-width: 30rem;
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  text-align: left;
}

.address-field label {
  font-size: 0.8rem;
  color: var(--text-dim);
}

.address-field input {
  width: 100%;
  padding: 0.6rem 0.8rem;
  font-size: 0.95rem;
  font-family: ui-monospace, SFMono-Regular, monospace;
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 0.5rem;
  background: var(--surface);
  color: var(--text);
}

.packs-empty {
  color: var(--text-dim);
  font-size: 0.85rem;
}

.packs-list {
  width: 100%;
  max-width: 30rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.pack-row {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  align-items: flex-start;
  background: var(--surface);
  border-radius: 0.6rem;
  padding: 0.75rem 1rem;
  text-align: left;
}

.pack-name {
  font-size: 0.95rem;
  font-weight: 600;
}

.pack-progress {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  width: 100%;
}

.pack-progress progress {
  flex: 1;
  height: 0.5rem;
}

.pack-status {
  font-size: 0.75rem;
  color: var(--text-dim);
  white-space: nowrap;
}

.pack-status-done {
  color: #4ade80;
}

.pack-status-error {
  color: var(--record);
}

.sync-button {
  align-self: flex-end;
  padding: 0.5rem 1.2rem;
  font-size: 0.9rem;
  font-weight: 600;
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 999px;
  background: none;
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.sync-button:active {
  background: var(--surface-2);
}

.sync-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.server-data-footer {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.server-data-size {
  font-size: 0.8rem;
  color: var(--text-dim);
  font-variant-numeric: tabular-nums;
}

.clear-button {
  padding: 0.7rem 1.4rem;
  font-size: 0.9rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 999px;
  background: none;
  color: var(--record);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.clear-button:active {
  background: var(--surface-2);
}

.clear-button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>
