<template>
  <svg v-if="!playing" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
    <path d="M7 5.2v13.6c0 .82.9 1.32 1.59.88l10.7-6.8a1.05 1.05 0 0 0 0-1.76l-10.7-6.8A1.05 1.05 0 0 0 7 5.2z" fill="currentColor" />
  </svg>
  <svg v-else viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
    <rect x="6" y="4.5" width="4" height="15" rx="1" fill="currentColor" />
    <rect x="14" y="4.5" width="4" height="15" rx="1" fill="currentColor" />
  </svg>
</template>

<script setup>
defineProps({
  playing: { type: Boolean, default: false },
})
</script>
