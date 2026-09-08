<template>
  <p v-if="shadowLoop.isActive" class="phase-label" :class="loopPhaseClass">{{ loopPhaseLabel }}</p>

  <div class="repeat-group">
    <span class="repeat-label">Repeat</span>
    <div class="repeat-buttons">
      <button
        v-for="n in [1, 2, 3]"
        :key="n"
        class="repeat-button"
        :class="{ selected: repeatCount === n }"
        :disabled="shadowLoop.isActive"
        @click="repeatCount = n"
      >{{ n }}</button>
    </div>
  </div>

  <button
    class="toggle-button"
    :class="{ on: repeatModel }"
    :disabled="shadowLoop.isActive"
    @click="repeatModel = !repeatModel"
  >
    Repeat model: {{ repeatModel ? 'On' : 'Off' }}
  </button>

  <button
    class="primary-button"
    :class="{ stop: shadowLoop.isActive }"
    @click="shadowLoop.isActive ? shadowLoop.stop() : handleStart()"
  >
    {{ shadowLoop.isActive ? 'Stop' : 'Start' }}
  </button>

  <p class="loop-item-count">{{ items.length }} {{ items.length === 1 ? 'word' : 'words' }}</p>

  <div v-if="shadowLoop.currentItem" class="loop-current-item">
    <slot name="current-item" :item="shadowLoop.currentItem"></slot>
  </div>
</template>

<script setup>
// Shared ShadowLoopMode UI (options + running view) - see
// docs/shadow-loop-mode-spec.md. Embedded by any screen with its own list
// of things to shadow (Flashcards' current Practice set, a YT Shadowing
// transcript's filtered vocabulary) - the host owns its own `items`/`speak`
// and its own entry point (a header Loop/Exit toggle, in both current
// uses), and just conditionally mounts this component via v-if; unmounting
// it (flipping that ref off) is enough to stop and clean up an active run,
// since useShadowLoop's own onBeforeUnmount handles that automatically -
// no manual stop() call needed from the host.
//
// How to render "the current item" is entirely the host's call (a
// flashcard's front+back are a very different shape from a bare
// transcript word) - exposed via the `current-item` scoped slot rather
// than baked in here.
import { computed, ref } from 'vue'
import { useShadowLoop } from '../composables/useShadowLoop.js'

const props = defineProps({
  items: { type: Array, required: true },
  speak: { type: Function, required: true },
})

const repeatCount = ref(1)
const repeatModel = ref(false)

const shadowLoop = useShadowLoop({ speak: (item) => props.speak(item) })

const loopPhaseLabel = computed(() => {
  switch (shadowLoop.loopPhase) {
    case 'speaking':
    case 'replaying-model':
      return 'Playing…'
    case 'recording':
      return 'Listening…'
    case 'playing-back':
      return 'Playing back…'
    default:
      return ''
  }
})

// Matches Robot Shadowing's own phase-label coloring convention exactly.
const loopPhaseClass = computed(() => {
  switch (shadowLoop.loopPhase) {
    case 'recording':
      return 'is-recording'
    case 'playing-back':
      return 'is-playing'
    case 'speaking':
    case 'replaying-model':
      return 'is-speaking'
    default:
      return ''
  }
})

function handleStart() {
  shadowLoop.start(props.items, {
    repeatCount: repeatCount.value,
    repeatModel: repeatModel.value,
  })
}
</script>

<style scoped>
.loop-item-count {
  font-size: 0.8rem;
  color: var(--text-dim);
  margin: 0;
}

.loop-current-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.75rem;
  margin-top: 1rem;
  text-align: center;
}
</style>
