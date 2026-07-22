<template>
  <button
    class="record-button"
    :class="{ recording: recordActive, compact }"
    :disabled="recordDisabled"
    @click="$emit('toggle-record')"
  >
    <span class="record-icon">
      <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
        <circle cx="12" cy="12" r="7" fill="currentColor" />
      </svg>
    </span>
    <span>{{ recordLabel }}</span>
  </button>

  <button
    class="shadow-button"
    :class="{ active: shadowActive, compact }"
    :disabled="shadowDisabled"
    @pointerdown="$emit('shadow-pointerdown', $event)"
    @pointerup="$emit('shadow-pointerup', $event)"
    @pointercancel="$emit('shadow-pointercancel', $event)"
    @click="$emit('shadow-click', $event)"
  >
    <span class="shadow-icon">
      <svg viewBox="0 0 24 24" fill="none">
        <g transform="translate(3,1)" fill="#ff5d5d" opacity="0.55">
          <circle cx="12" cy="8" r="3.4" />
          <path d="M6 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
        </g>
        <g fill="currentColor">
          <circle cx="9" cy="8" r="3.4" />
          <path d="M3 19c0-3.6 2.7-6 6-6s6 2.4 6 6" />
        </g>
      </svg>
    </span>
    <span>{{ shadowLabel }}</span>
  </button>
</template>

<script setup>
defineProps({
  recordLabel: { type: String, required: true },
  recordActive: { type: Boolean, default: false },
  recordDisabled: { type: Boolean, default: false },
  shadowLabel: { type: String, required: true },
  shadowActive: { type: Boolean, default: false },
  shadowDisabled: { type: Boolean, default: false },
  compact: { type: Boolean, default: false },
})

defineEmits([
  'toggle-record',
  'shadow-click',
  'shadow-pointerdown',
  'shadow-pointerup',
  'shadow-pointercancel',
])
</script>

<style scoped>
.record-button,
.shadow-button {
  flex: 1 1 auto;
  min-width: 6rem;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  padding: 0.8rem 0.6rem;
  font-size: 0.9rem;
  font-weight: 700;
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 999px;
  background: var(--surface);
  color: var(--text);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  -webkit-touch-callout: none;
  touch-action: manipulation;
  transition: background 0.15s ease, transform 0.15s ease;
}

.record-button:active,
.shadow-button:active {
  transform: scale(0.98);
}

.record-button:disabled,
.shadow-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.record-button.recording {
  background: var(--record);
  border-color: transparent;
  color: #fff;
}

.record-icon {
  display: inline-flex;
  width: 1.1rem;
  height: 1.1rem;
  color: var(--record);
}

.record-button.recording .record-icon {
  color: #fff;
  animation: record-icon-pulse 1s ease-in-out infinite;
}

.record-icon svg {
  width: 100%;
  height: 100%;
}

@keyframes record-icon-pulse {
  0%,
  100% {
    opacity: 1;
  }
  50% {
    opacity: 0.35;
  }
}

/* Compact: sized to fit alongside the word popup's other header buttons on
   one line - flex-grow: 0 so it doesn't stretch to fill whatever space is
   left on the row (that's what made it balloon in the popup, where only 2
   buttons share the row, versus 3 sharing it in the main screen). */
.record-button.compact,
.shadow-button.compact {
  flex: 0 0 auto;
  min-width: 0;
  padding: 0.35rem 0.6rem;
  font-size: 0.72rem;
  gap: 0.3rem;
}

.record-button.compact .record-icon {
  width: 0.85rem;
  height: 0.85rem;
}

.shadow-button.compact .shadow-icon {
  width: 0.9rem;
  height: 0.9rem;
}

.shadow-button.active {
  background: var(--accent-gradient);
  border-color: transparent;
  color: #fff;
}

.shadow-icon {
  display: inline-flex;
  width: 1.15rem;
  height: 1.15rem;
  flex-shrink: 0;
}

.shadow-icon svg {
  width: 100%;
  height: 100%;
}
</style>
