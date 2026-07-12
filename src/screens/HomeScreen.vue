<template>
  <main class="screen">
    <img class="app-logo" :src="`${logoBase}icons/icon-192.png`" alt="" />
    <h1>English Practice</h1>

    <ul class="tool-list">
      <li>
        <button
          class="tool-button"
          @click="$emit('select-tool', 'recorder-loop')"
        >
          <span class="tool-icon">
            <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
              <path
                d="M6 10v4M9 7v10M12 4v16M15 7v10M18 10v4"
                stroke="white"
                stroke-width="2"
                stroke-linecap="round"
              />
            </svg>
          </span>
          <span class="tool-label">Recorder Loop</span>
          <span class="tool-chevron">›</span>
        </button>
      </li>
      <li>
        <button
          class="tool-button"
          @click="$emit('select-tool', 'robot-shadowing')"
        >
          <span class="tool-icon">
            <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
              <path
                d="M12 4a3 3 0 013 3v4a3 3 0 01-6 0V7a3 3 0 013-3z"
                stroke="white"
                stroke-width="2"
              />
              <path
                d="M6 11a6 6 0 0012 0M12 17v3"
                stroke="white"
                stroke-width="2"
                stroke-linecap="round"
              />
            </svg>
          </span>
          <span class="tool-label">Robot Shadowing</span>
          <span class="tool-chevron">›</span>
        </button>
      </li>
    </ul>
  </main>
</template>

<script setup>
defineEmits(['select-tool'])

const logoBase = import.meta.env.BASE_URL
</script>
