<template>
  <div class="confirm-backdrop" v-if="visible" @click.self="handleCancel">
    <div class="confirm-dialog" role="alertdialog" aria-modal="true">
      <p class="confirm-message">{{ message }}</p>
      <div class="confirm-buttons">
        <button class="cancel-button" @click="handleCancel">{{ cancelLabel }}</button>
        <button class="danger-button" @click="handleConfirm">{{ confirmLabel }}</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'

const visible = ref(false)
const message = ref('')
const confirmLabel = ref('Confirm')
const cancelLabel = ref('Cancel')

let resolvePromise = null

// Returns a Promise<boolean> - true if the user confirmed, false if they
// cancelled (either button, clicking the backdrop, or Escape).
function confirm(text, options = {}) {
  message.value = text
  confirmLabel.value = options.confirmLabel ?? 'Confirm'
  cancelLabel.value = options.cancelLabel ?? 'Cancel'
  visible.value = true

  return new Promise((resolve) => {
    resolvePromise = resolve
  })
}

function settle(result) {
  visible.value = false
  resolvePromise?.(result)
  resolvePromise = null
}

function handleConfirm() {
  settle(true)
}

function handleCancel() {
  settle(false)
}

function handleKeydown(event) {
  if (visible.value && event.key === 'Escape') handleCancel()
}

onMounted(() => {
  window.addEventListener('keydown', handleKeydown)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleKeydown)
})

defineExpose({ confirm })
</script>

<style scoped>
.confirm-backdrop {
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

.confirm-dialog {
  width: 100%;
  max-width: 20rem;
  display: flex;
  flex-direction: column;
  gap: 1.25rem;
  padding: 1.5rem;
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 1rem;
  background: var(--surface);
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
}

.confirm-message {
  margin: 0;
  font-size: 1.05rem;
  line-height: 1.4;
  text-align: center;
}

.confirm-buttons {
  display: flex;
  gap: 0.75rem;
}

.cancel-button,
.danger-button {
  flex: 1;
  padding: 0.7rem 1rem;
  font-size: 0.95rem;
  font-weight: 600;
  border: none;
  border-radius: 999px;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  transition: transform 0.15s ease;
}

.cancel-button:active,
.danger-button:active {
  transform: scale(0.96);
}

.cancel-button {
  background: var(--surface-2);
  color: var(--text);
}

.danger-button {
  background: var(--record);
  color: #fff;
}
</style>
