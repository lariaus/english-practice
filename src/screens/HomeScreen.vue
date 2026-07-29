<template>
  <main class="screen">
    <button class="settings-button" aria-label="Settings" @click="$emit('open-settings')">
      <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
        <circle cx="12" cy="12" r="3" stroke="currentColor" stroke-width="2" />
        <path
          d="M19.4 13a7.97 7.97 0 000-2l2.1-1.6-2-3.4-2.5 1a8 8 0 00-1.7-1L14.8 3h-4l-.5 2.9a8 8 0 00-1.7 1l-2.5-1-2 3.4L6.2 11a7.97 7.97 0 000 2l-2.1 1.6 2 3.4 2.5-1a8 8 0 001.7 1l.5 2.9h4l.5-2.9a8 8 0 001.7-1l2.5 1 2-3.4L19.4 13z"
          stroke="currentColor"
          stroke-width="1.6"
          stroke-linejoin="round"
        />
      </svg>
    </button>

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
              <path d="M12 4v2" stroke="white" stroke-width="2" stroke-linecap="round" />
              <circle cx="12" cy="3" r="1.3" fill="white" />
              <rect x="5" y="8" width="14" height="11" rx="3" stroke="white" stroke-width="2" />
              <circle cx="9" cy="13.5" r="1.4" fill="white" />
              <circle cx="15" cy="13.5" r="1.4" fill="white" />
              <path d="M9 17h6" stroke="white" stroke-width="2" stroke-linecap="round" />
            </svg>
          </span>
          <span class="tool-label">Robot Shadowing</span>
          <span class="tool-chevron">›</span>
        </button>
      </li>
      <li>
        <button
          class="tool-button"
          @click="$emit('select-tool', 'yt-shadowing')"
        >
          <span class="tool-icon">
            <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
              <path
                d="M10 8l6 4-6 4V8z"
                fill="white"
              />
              <rect
                x="3"
                y="5"
                width="18"
                height="14"
                rx="3"
                stroke="white"
                stroke-width="2"
              />
            </svg>
          </span>
          <span class="tool-label">YT Shadowing</span>
          <span class="tool-chevron">›</span>
        </button>
      </li>
      <li>
        <!-- Doesn't navigate anywhere - opens the dictionary as a popup
             overlay on top of this same screen (see docs/dictionary-spec.md),
             unlike the three tools above which switch to a whole new
             screen. Same look/row as the others otherwise. -->
        <button
          class="tool-button"
          @click="$emit('open-dictionary')"
        >
          <span class="tool-icon">
            <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
              <path
                d="M12 6c-1.5-1.3-3.5-2-6-2v13c2.5 0 4.5.7 6 2 1.5-1.3 3.5-2 6-2V4c-2.5 0-4.5.7-6 2z"
                stroke="white"
                stroke-width="2"
                stroke-linejoin="round"
              />
              <path d="M12 6v13" stroke="white" stroke-width="2" />
            </svg>
          </span>
          <span class="tool-label">Dictionary</span>
          <span class="tool-chevron">›</span>
        </button>
      </li>
    </ul>
  </main>
</template>

<script setup>
defineEmits(['select-tool', 'open-settings', 'open-dictionary'])

const logoBase = import.meta.env.BASE_URL
</script>

<style scoped>
/* Mirrors .back-button's positioning (style.css) but top-right instead of
   top-left, since this screen has no back button of its own to conflict
   with. */
.settings-button {
  position: absolute;
  top: max(1.25rem, env(safe-area-inset-top));
  right: 1.25rem;
  width: 2rem;
  height: 2rem;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  border: none;
  border-radius: 50%;
  background: none;
  color: var(--text-dim);
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
}

.settings-button svg {
  width: 1.3rem;
  height: 1.3rem;
}
</style>
