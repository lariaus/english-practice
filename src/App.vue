<template>
  <HomeScreen v-if="screen.name === 'home'" @select-tool="onSelectTool" />

  <DurationPickerScreen
    v-else-if="screen.name === 'recorder-loop-duration'"
    title="Recorder Loop"
    @select-duration="onSelectRecorderDuration"
    @back="goHome"
  />

  <RecorderLoopSessionScreen
    v-else-if="screen.name === 'recorder-loop-session'"
    :duration="screen.duration"
    @back="goToRecorderDurationPicker"
  />

  <RobotShadowingSessionScreen
    v-else-if="screen.name === 'robot-shadowing-session'"
    @back="goHome"
  />
</template>

<script setup>
import { ref } from 'vue'
import HomeScreen from './screens/HomeScreen.vue'
import DurationPickerScreen from './screens/DurationPickerScreen.vue'
import RecorderLoopSessionScreen from './screens/RecorderLoopSessionScreen.vue'
import RobotShadowingSessionScreen from './screens/RobotShadowingSessionScreen.vue'

const screen = ref({ name: 'home' })

function onSelectTool(toolId) {
  if (toolId === 'recorder-loop') {
    screen.value = { name: 'recorder-loop-duration' }
  } else if (toolId === 'robot-shadowing') {
    screen.value = { name: 'robot-shadowing-session' }
  }
}

function onSelectRecorderDuration(seconds) {
  screen.value = { name: 'recorder-loop-session', duration: seconds }
}

function goHome() {
  screen.value = { name: 'home' }
}

function goToRecorderDurationPicker() {
  screen.value = { name: 'recorder-loop-duration' }
}
</script>
