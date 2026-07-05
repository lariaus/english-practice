<template>
  <HomeScreen v-if="screen.name === 'home'" @select-tool="onSelectTool" />

  <RecorderLoopDurationScreen
    v-else-if="screen.name === 'recorder-loop-duration'"
    @select-duration="onSelectDuration"
    @back="goHome"
  />

  <RecorderLoopSessionScreen
    v-else-if="screen.name === 'recorder-loop-session'"
    :duration="screen.duration"
    @back="goToDurationPicker"
  />
</template>

<script setup>
import { ref } from 'vue'
import HomeScreen from './screens/HomeScreen.vue'
import RecorderLoopDurationScreen from './screens/RecorderLoopDurationScreen.vue'
import RecorderLoopSessionScreen from './screens/RecorderLoopSessionScreen.vue'

const screen = ref({ name: 'home' })

function onSelectTool(toolId) {
  if (toolId === 'recorder-loop') {
    screen.value = { name: 'recorder-loop-duration' }
  }
}

function onSelectDuration(seconds) {
  screen.value = { name: 'recorder-loop-session', duration: seconds }
}

function goHome() {
  screen.value = { name: 'home' }
}

function goToDurationPicker() {
  screen.value = { name: 'recorder-loop-duration' }
}
</script>
