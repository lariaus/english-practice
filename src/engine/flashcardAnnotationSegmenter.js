// Splits text into alternating plain/annotation segments, wherever a
// `/.../ ` phonetic transcription or a `(...)` parenthetical annotation
// appears (e.g. "record (noun), /ˈrek.ɚd/") - both get their own display
// treatment in FlashcardText.vue (see docs/flashcards-spec.md), and
// neither is individually word-clickable for a dictionary lookup:
// "/rɪˈpɔːr.t̬ɪd.li/" isn't a real word, and "(noun)" is a note about the
// word, not the word itself.
//
// A single combined regex (not two separate passes) so the two kinds can
// appear in any order/count relative to each other and to plain text -
// "text (paren) /ipa/", "/ipa/(paren)/ipa/", etc. all fall out naturally
// from scanning left to right for the next match of either kind.
const ANNOTATION_RE = /\/[^/]+\/|\([^)]+\)/g

// list[{type: 'text' | 'ipa' | 'paren', value: str}], in order, covering
// the whole input with no gaps.
export function splitIntoAnnotatedSegments(text) {
  const segments = []
  let lastIndex = 0
  let match

  ANNOTATION_RE.lastIndex = 0
  while ((match = ANNOTATION_RE.exec(text))) {
    if (match.index > lastIndex) {
      segments.push({ type: 'text', value: text.slice(lastIndex, match.index) })
    }
    const value = match[0]
    segments.push({ type: value.startsWith('/') ? 'ipa' : 'paren', value })
    lastIndex = match.index + value.length
  }
  if (lastIndex < text.length) {
    segments.push({ type: 'text', value: text.slice(lastIndex) })
  }
  return segments
}

// What's actually left to say aloud - drops every ipa/paren segment
// entirely, joins what remains, and collapses the whitespace a removed
// middle segment leaves behind (e.g. "abc (note) def" -> "abc  def" ->
// "abc def"). "record (noun)" -> "record"; a card whose text is *only*
// annotation (e.g. a pure IPA back side, "/ˈhɪs.t̬ɚ.i/") strips down to
// "" - see useFlashcardFaceAudio.js, which hides the whole Play/Record/
// Shadow trio whenever this comes back empty, rather than letting TTS
// speak nothing.
export function stripAnnotations(text) {
  return splitIntoAnnotatedSegments(text)
    .filter((segment) => segment.type === 'text')
    .map((segment) => segment.value)
    .join('')
    .replace(/\s+/g, ' ')
    .trim()
}
