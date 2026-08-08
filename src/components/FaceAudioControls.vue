<template>
  <div v-if="audio.hasSpeakableText" class="face-audio-buttons" @click.stop>
    <PlayButton compact @click="audio.handlePlayClick" />
    <RecordShadowButtons
      compact
      :record-label="audio.recordLabel"
      :record-active="audio.recordSessionActive && audio.micState.phase === 'recording'"
      :record-disabled="audio.isShadowing"
      :shadow-label="audio.shadowLabel"
      :shadow-active="audio.isShadowing"
      :shadow-disabled="audio.recordSessionActive"
      @toggle-record="audio.toggleRecording($event)"
      @record-pointerdown="audio.handleRecordPointerDown"
      @record-pointerup="audio.handleRecordPointerUp"
      @record-pointercancel="audio.handleRecordPointerCancel"
      @shadow-click="audio.handleShadowClick"
      @shadow-pointerdown="audio.handleShadowPointerDown"
      @shadow-pointerup="audio.handleShadowPointerUp"
      @shadow-pointercancel="audio.handleShadowPointerCancel"
    />
  </div>
</template>

<script setup>
import RecordShadowButtons from './RecordShadowButtons.vue'
import PlayButton from './PlayButton.vue'

// `audio` is a useFlashcardFaceAudio() instance - a reactive object, so its
// nested refs (recordLabel, micState, etc.) unwrap correctly in the
// template above without needing .value. Renders nothing at all when
// audio.hasSpeakableText is false (e.g. a face whose text is pure IPA/
// paren annotation, stripping down to nothing to actually say).
defineProps({
  audio: { type: Object, required: true },
})
</script>
