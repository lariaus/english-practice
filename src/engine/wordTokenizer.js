// Splits text into alternating word/whitespace tokens, preserving exact
// spacing so it can be re-rendered with only the word tokens made
// individually clickable - shared by YT Shadowing's transcript and
// Flashcards' front/back text.
//
// Unicode-aware (\p{L}/\p{N}, not \w) - \w is ASCII-only ([A-Za-z0-9_]), so
// it would treat accented letters (café, garçon) or IPA characters as
// punctuation and strip them at a token's edge. This matters a lot more
// here than it ever did for English YouTube captions, since Flashcards is
// explicitly used for French vocabulary and phonetic spellings.
export function splitIntoWords(text) {
  return text.split(/(\s+)/)
}

export function isClickableWord(token) {
  return /[\p{L}\p{N}]/u.test(token)
}

// Strips leading/trailing punctuation from a token, keeping internal
// apostrophes (e.g. "l'arbre" stays whole).
export function cleanWord(token) {
  return token.replace(/^[^\p{L}\p{N}']+|[^\p{L}\p{N}']+$/gu, '')
}
