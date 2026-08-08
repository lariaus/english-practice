<template>
  <button class="play-button" :class="{ compact }" @click="$emit('click', $event)" aria-label="Play">
    <PlayPauseIcon />
  </button>
</template>

<script setup>
import PlayPauseIcon from './PlayPauseIcon.vue'

defineProps({
  compact: { type: Boolean, default: false },
})
defineEmits(['click'])
</script>

<style scoped>
/* Same look as DictionaryPopup.vue's .dictionary-play-button - kept as its
   own small component instead, since it's now also used outside that
   popup (Flashcards' front/back Play buttons). */
.play-button {
  flex-shrink: 0;
  width: 1.6rem;
  height: 1.6rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border: none;
  border-radius: 50%;
  background: var(--surface-2);
  color: var(--accent-1);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.play-button :deep(svg) {
  width: 0.7rem;
  height: 0.7rem;
}

.play-button:active {
  transform: scale(0.92);
}

.play-button.compact {
  width: 1.3rem;
  height: 1.3rem;
}

.play-button.compact :deep(svg) {
  width: 0.6rem;
  height: 0.6rem;
}
</style>
