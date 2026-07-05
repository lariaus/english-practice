<template>
  <main class="screen">
    <button class="back-button" @click="$emit('back')">&larr; Back</button>

    <h1>Recorder Loop</h1>
    <p class="subtitle">Choose a loop duration</p>

    <div class="duration-grid">
      <button
        v-for="d in durations"
        :key="d"
        class="duration-button"
        @click="$emit('select-duration', d)"
      >
        {{ d }}s
      </button>
    </div>
  </main>
</template>

<script setup>
defineEmits(['select-duration', 'back'])

const durations = [10, 15, 30, 60, 90]
</script>
