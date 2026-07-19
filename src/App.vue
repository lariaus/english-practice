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

  <YtShadowingFormScreen
    v-else-if="screen.name === 'yt-shadowing-form'"
    @back="goHome"
    @load="onLoadYtVideo"
  />

  <YtShadowingPlayerScreen
    v-else-if="screen.name === 'yt-shadowing-player'"
    :video-id="screen.videoId"
    :url="screen.url"
    @back="goToYtShadowingForm"
  />
</template>

<script setup>
import { ref } from 'vue'
import HomeScreen from './screens/HomeScreen.vue'
import DurationPickerScreen from './screens/DurationPickerScreen.vue'
import RecorderLoopSessionScreen from './screens/RecorderLoopSessionScreen.vue'
import RobotShadowingSessionScreen from './screens/RobotShadowingSessionScreen.vue'
import YtShadowingFormScreen from './screens/YtShadowingFormScreen.vue'
import YtShadowingPlayerScreen from './screens/YtShadowingPlayerScreen.vue'

const screen = ref({ name: 'home' })

function onSelectTool(toolId) {
  if (toolId === 'recorder-loop') {
    screen.value = { name: 'recorder-loop-duration' }
  } else if (toolId === 'robot-shadowing') {
    screen.value = { name: 'robot-shadowing-session' }
  } else if (toolId === 'yt-shadowing') {
    screen.value = { name: 'yt-shadowing-form' }
  }
}

function onSelectRecorderDuration(seconds) {
  screen.value = { name: 'recorder-loop-session', duration: seconds }
}

function onLoadYtVideo({ videoId, url }) {
  screen.value = { name: 'yt-shadowing-player', videoId, url }
}

function goHome() {
  screen.value = { name: 'home' }
}

function goToRecorderDurationPicker() {
  screen.value = { name: 'recorder-loop-duration' }
}

function goToYtShadowingForm() {
  screen.value = { name: 'yt-shadowing-form' }
}
</script>
