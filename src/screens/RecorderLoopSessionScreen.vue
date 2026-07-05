<template>
  <main class="screen recorder-screen">
    <button class="back-button" @click="handleBack">&larr; Back</button>

    <h1>Record Loop {{ duration }}s</h1>

    <p v-if="state.error" class="error-message">{{ state.error }}</p>

    <template v-else>
      <p class="phase-label" :class="phaseClass">{{ phaseLabel }}</p>

      <div v-if="showCountdown" class="countdown-ring">
        <svg viewBox="0 0 120 120" class="countdown-svg">
          <circle class="countdown-track" cx="60" cy="60" r="54" />
          <circle
            class="countdown-progress"
            :class="phaseClass"
            cx="60"
            cy="60"
            r="54"
            :style="progressStyle"
          />
        </svg>
        <span class="countdown-number">{{ state.secondsRemaining }}</span>
      </div>
    </template>

    <button class="primary-button" :class="{ stop: isActive }" @click="toggle">
      {{ isActive ? 'Stop' : 'Start' }}
    </button>
  </main>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, reactive } from 'vue'
import { RecorderLoopEngine } from '../engine/recorderLoopEngine.js'

const props = defineProps({
  duration: { type: Number, required: true },
})
const emit = defineEmits(['back'])

const state = reactive({
  phase: 'idle',
  secondsRemaining: props.duration,
  totalSeconds: props.duration,
  error: null,
})

let engine

onMounted(() => {
  engine = new RecorderLoopEngine(props.duration, {
    onChange: (snapshot) => Object.assign(state, snapshot),
  })
})

onBeforeUnmount(() => {
  engine?.destroy()
})

const isActive = computed(() =>
  ['requesting-permission', 'recording', 'playing'].includes(state.phase),
)

const showCountdown = computed(() =>
  ['recording', 'playing'].includes(state.phase),
)

const phaseClass = computed(() => {
  if (state.phase === 'recording') return 'is-recording'
  if (state.phase === 'playing') return 'is-playing'
  return ''
})

const phaseLabel = computed(() => {
  switch (state.phase) {
    case 'requesting-permission':
      return 'Requesting microphone access…'
    case 'recording':
      return 'Recording'
    case 'playing':
      return 'Playing back'
    default:
      return 'Ready'
  }
})

const CIRCUMFERENCE = 2 * Math.PI * 54

const progressStyle = computed(() => {
  const ratio =
    state.totalSeconds > 0
      ? (state.totalSeconds - state.secondsRemaining) / state.totalSeconds
      : 0
  const offset = CIRCUMFERENCE * (1 - ratio)
  return {
    strokeDasharray: `${CIRCUMFERENCE}`,
    strokeDashoffset: `${offset}`,
  }
})

function toggle() {
  if (isActive.value) {
    engine.stop()
  } else {
    engine.start()
  }
}

function handleBack() {
  engine?.stop()
  emit('back')
}
</script>
