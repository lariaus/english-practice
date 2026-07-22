// Shared "did this press ask for the alternate behavior?" gesture: a
// shift-click (desktop) or a long-press (touch, no shift key available) are
// treated as equivalent. Used wherever a plain click means one thing and
// shift-click/long-press means another - e.g. Shadow's double pass, or
// Play's temporary slow-speed playback.
const DEFAULT_LONG_PRESS_MS = 500

export function useShiftOrLongPress(longPressMs = DEFAULT_LONG_PRESS_MS) {
  let timer = null
  let fired = false

  function clearTimer() {
    if (timer) {
      clearTimeout(timer)
      timer = null
    }
  }

  // `guard`, if given, must return true for the long-press timer to start -
  // e.g. only meaningful right before a fresh action would actually begin,
  // not while it's already mid-flight.
  function handlePointerDown(guard) {
    if (guard && !guard()) return
    fired = false
    timer = setTimeout(() => {
      fired = true
    }, longPressMs)
  }

  function handlePointerUp() {
    clearTimer()
  }

  function handlePointerCancel() {
    clearTimer()
    fired = false
  }

  // Reads (and resets) whether this press requested the alternate behavior -
  // shift-click or the long-press detected above. Only meaningful right at
  // the press that's actually about to act on it.
  function consume(event) {
    const result = !!event?.shiftKey || fired
    fired = false
    return result
  }

  return { handlePointerDown, handlePointerUp, handlePointerCancel, consume }
}
