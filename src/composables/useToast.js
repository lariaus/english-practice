// A small global toast queue - call showToast() from anywhere (any
// component or plain .js module, no template ref/prop-drilling needed) to
// pop up a brief, self-dismissing message. Mirrors the classic iOS/Android
// "toast"/"snackbar" pattern: a dark pill near the bottom of the screen
// that fades in, holds for a few seconds, then fades out on its own - no
// tap-to-dismiss needed. See ToastHost.vue for the actual rendering,
// mounted once at the app root (App.vue) so it overlays whatever screen is
// currently showing.
//
// Deliberately not (yet) a replacement for every screen's persistent
// `error-message` paragraph - this is for a transient *action* failing
// (add/edit/delete, etc.), not for "this screen has nothing to show,"
// which still deserves a permanent, visible explanation rather than
// something that fades away in a few seconds.
import { reactive } from 'vue'

const DEFAULT_DURATION_MS = 3000

let nextId = 1

export const toastState = reactive({ toasts: [] })

export function showToast(message, { type = 'default', duration = DEFAULT_DURATION_MS } = {}) {
  const id = nextId++
  toastState.toasts.push({ id, message, type })
  setTimeout(() => {
    const index = toastState.toasts.findIndex((t) => t.id === id)
    if (index !== -1) toastState.toasts.splice(index, 1)
  }, duration)
}
