// Opens external dictionary/reference sites for a word in a small centered
// popup window - shared so both WordInfoPopup (its "Ca"/"Wr" links) and
// YtShadowingPlayerScreen (its shift-click-a-word shortcut) open the same
// site the same way, rather than each keeping its own copy.

const REFERENCE_SITES = {
  cambridge: (word) => `https://dictionary.cambridge.org/dictionary/english/${encodeURIComponent(word)}`,
  wordreference: (word) => `https://www.wordreference.com/definition/${encodeURIComponent(word)}`,
}

export function openCenteredWindow(url) {
  const width = 700
  const height = 800
  const left = window.screenX + (window.outerWidth - width) / 2
  const top = window.screenY + (window.outerHeight - height) / 2
  window.open(url, '_blank', `width=${width},height=${height},left=${left},top=${top}`)
}

export function openReferenceSite(siteId, word) {
  const buildUrl = REFERENCE_SITES[siteId]
  if (!buildUrl || !word) return
  openCenteredWindow(buildUrl(word))
}
