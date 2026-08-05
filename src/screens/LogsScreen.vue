<template>
  <main class="screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1>Logs</h1>

    <div class="logs-toolbar">
      <span class="logs-count">{{ logEntries.length }} entries</span>
      <button class="clear-button" @click="clearLogs">Clear</button>
    </div>

    <div ref="logListRef" class="logs-list">
      <p v-if="logEntries.length === 0" class="logs-empty">Nothing logged yet.</p>
      <div v-for="(entry, index) in logEntries" :key="index" class="logs-entry">
        <span class="logs-time">{{ entry.time }}</span>
        <span class="logs-text">{{ entry.text }}</span>
      </div>
    </div>
  </main>
</template>

<script setup>
import { nextTick, ref, watch } from 'vue'
import { clearLogs, logEntries } from '../engine/appLog.js'

defineEmits(['back'])

const logListRef = ref(null)

watch(
  () => logEntries.value.length,
  async () => {
    await nextTick()
    if (logListRef.value) {
      logListRef.value.scrollTop = logListRef.value.scrollHeight
    }
  },
)
</script>

<style scoped>
.logs-toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  max-width: 30rem;
}

.logs-count {
  font-size: 0.85rem;
  color: var(--text-dim);
}

.clear-button {
  padding: 0.5rem 1rem;
  font-size: 0.85rem;
  font-weight: 600;
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 999px;
  background: none;
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.clear-button:active {
  background: var(--surface-2);
}

.logs-list {
  width: 100%;
  max-width: 30rem;
  flex: 1;
  overflow-y: auto;
  text-align: left;
  background: var(--surface);
  border-radius: 0.6rem;
  padding: 0.75rem;
}

.logs-empty {
  color: var(--text-dim);
  font-size: 0.85rem;
}

.logs-entry {
  display: flex;
  gap: 0.5rem;
  font-family: ui-monospace, SFMono-Regular, monospace;
  font-size: 0.75rem;
  line-height: 1.4;
  padding: 0.15rem 0;
  word-break: break-word;
}

.logs-time {
  flex-shrink: 0;
  color: var(--text-dim);
}

.logs-text {
  white-space: pre-wrap;
}
</style>
