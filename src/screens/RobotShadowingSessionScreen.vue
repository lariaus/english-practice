<template>
  <main class="screen recorder-screen">
    <button class="back-button" @click="handleBack">&larr; Back</button>

    <h1>Robot Shadowing</h1>

    <p v-if="state.error" class="error-message">{{ state.error }}</p>

    <p v-else class="phase-label" :class="phaseClass">{{ phaseLabel }}</p>

    <select class="voice-select" v-model="selectedVoiceId" :disabled="isActive">
      <option v-for="option in TTS_VOICE_OPTIONS" :key="option.id" :value="option.id">
        {{ option.label }}
      </option>
    </select>

    <select class="voice-select" v-model="selectedDatabaseId" :disabled="isActive">
      <option v-for="option in PHRASE_DATABASE_OPTIONS" :key="option.id" :value="option.id">
        {{ option.label }}
      </option>
    </select>

    <div class="repeat-group">
      <span class="repeat-label">Repeat</span>
      <div class="repeat-buttons">
        <button
          v-for="n in [1, 2, 3]"
          :key="n"
          class="repeat-button"
          :class="{ selected: repeatCount === n }"
          :disabled="isActive"
          @click="repeatCount = n"
        >
          {{ n }}
        </button>
      </div>
    </div>

    <button
      class="toggle-button"
      :class="{ on: repeatModel }"
      :disabled="isActive"
      @click="repeatModel = !repeatModel"
    >
      Repeat model: {{ repeatModel ? 'On' : 'Off' }}
    </button>

    <button class="primary-button" :class="{ stop: isActive }" @click="toggle">
      {{ isActive ? 'Stop' : 'Start' }}
    </button>
  </main>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref } from 'vue'
import { RobotShadowingEngine, PHRASE_DATABASE_OPTIONS } from '../engine/robotShadowingEngine.js'
import { TTS_VOICE_OPTIONS } from '../engine/ttsEngine.js'

const emit = defineEmits(['back'])

const selectedVoiceId = ref('google-tts-api')
const selectedDatabaseId = ref('easy')
const repeatCount = ref(1)
const repeatModel = ref(false)

const state = reactive({
  phase: 'idle',
  error: null,
})

let engine

onMounted(() => {
  engine = new RobotShadowingEngine({
    onChange: (snapshot) => Object.assign(state, snapshot),
  })
})

onBeforeUnmount(() => {
  engine?.destroy()
})

const isActive = computed(() =>
  ['requesting-permission', 'speaking', 'recording', 'playing'].includes(state.phase),
)

const phaseClass = computed(() => {
  if (state.phase === 'recording') return 'is-recording'
  if (state.phase === 'playing') return 'is-playing'
  if (state.phase === 'speaking') return 'is-speaking'
  return ''
})

const phaseLabel = computed(() => {
  switch (state.phase) {
    case 'requesting-permission':
      return 'Requesting microphone access…'
    case 'speaking':
      return 'Listen…'
    case 'recording':
      return 'Your turn'
    case 'playing':
      return 'Playing back'
    default:
      return 'Ready'
  }
})

function toggle() {
  if (isActive.value) {
    engine.stop()
  } else {
    engine.start(selectedVoiceId.value, repeatCount.value, repeatModel.value, selectedDatabaseId.value)
  }
}

function handleBack() {
  engine?.stop()
  emit('back')
}
</script>
