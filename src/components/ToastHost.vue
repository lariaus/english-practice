<template>
  <div class="toast-host">
    <transition-group name="toast-fade">
      <p v-for="t in toastState.toasts" :key="t.id" class="toast" :class="t.type">{{ t.message }}</p>
    </transition-group>
  </div>
</template>

<script setup>
import { toastState } from '../composables/useToast.js'
</script>

<style scoped>
.toast-host {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 2.2rem;
  z-index: 1000;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
  pointer-events: none;
}

.toast {
  margin: 0;
  max-width: min(80vw, 24rem);
  padding: 0.7rem 1.2rem;
  border-radius: 999px;
  background: rgba(28, 28, 30, 0.92);
  backdrop-filter: blur(6px);
  color: #fff;
  font-size: 0.9rem;
  font-weight: 600;
  text-align: center;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.35);
}

.toast.error {
  color: var(--record);
}

.toast-fade-enter-active,
.toast-fade-leave-active {
  transition: opacity 0.25s ease, transform 0.25s ease;
}

.toast-fade-enter-from,
.toast-fade-leave-to {
  opacity: 0;
  transform: translateY(0.4rem);
}
</style>
