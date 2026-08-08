<template>
  <p class="flashcard-text">
    <template v-for="(segment, si) in segments" :key="si">
      <span v-if="segment.type === 'ipa'" class="ipa-text">{{ segment.value }}</span>
      <span v-else-if="segment.type === 'paren'" class="paren-text">{{ segment.value }}</span>
      <template v-else>
        <template v-for="(token, ti) in splitIntoWords(segment.value)" :key="ti">
          <span
            v-if="isClickableWord(token)"
            class="clickable-word"
            @click="$emit('word-click', token, $event)"
          >{{ token }}</span>
          <template v-else>{{ token }}</template>
        </template>
      </template>
    </template>
  </p>
</template>

<script setup>
// Renders a flashcard face's text with independent treatments layered
// together: IPA phonetic notation (/.../ ) and parenthetical annotations
// ((noun), etc.) each get their own de-emphasized display style and are
// never individually word-clickable (see flashcardAnnotationSegmenter.js
// and docs/flashcards-spec.md - a transcription or annotation isn't a real
// dictionary word to look up); everything else keeps the existing per-word
// Dictionary-popup clickability (wordTokenizer.js).
import { computed } from 'vue'
import { splitIntoAnnotatedSegments } from '../engine/flashcardAnnotationSegmenter.js'
import { isClickableWord, splitIntoWords } from '../engine/wordTokenizer.js'

const props = defineProps({
  text: { type: String, required: true },
})
defineEmits(['word-click'])

const segments = computed(() => splitIntoAnnotatedSegments(props.text))
</script>
