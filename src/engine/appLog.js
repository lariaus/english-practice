// Same call signature as console.log, but also keeps an in-memory ring
// buffer of recent entries so they're viewable from inside the app itself
// (Settings > Logs) - real iOS devices don't reliably surface console/
// native output anywhere else reachable during normal use, so this is the
// one place that always works, on every platform.

import { ref } from 'vue'

const MAX_ENTRIES = 500

export const logEntries = ref([])

function formatArg(arg) {
  if (typeof arg === 'string') return arg
  try {
    return JSON.stringify(arg)
  } catch {
    return String(arg)
  }
}

export function log(...args) {
  console.log(...args)
  logEntries.value.push({
    time: new Date().toLocaleTimeString(),
    text: args.map(formatArg).join(' '),
  })
  if (logEntries.value.length > MAX_ENTRIES) {
    logEntries.value.splice(0, logEntries.value.length - MAX_ENTRIES)
  }
}

export function clearLogs() {
  logEntries.value = []
}
