// Fetches word info (phonetics, definitions) for the click-a-word popup,
// shared across the app (not just YT Shadowing). Caches results in memory
// so re-looking-up the same word never re-fetches for the rest of the
// session. Only the Free Dictionary API (dictionaryapi.dev) for now - more
// sources (e.g. Merriam-Webster) may be added later behind this same
// fetchWordInfo() interface. Each phonetic entry carries its own audio URL
// (when available) - actually playing it is the view layer's job.

const API_BASE = 'https://api.dictionaryapi.dev/api/v2/entries'

const cache = new Map()

const REGION_LABELS = { us: 'US', uk: 'UK', au: 'AU' }

// Phonetics entries pair a `text` transcription with an `audio` file in the
// same object - when that audio filename ends in e.g. "-us.mp3" it's
// reliably that dialect's transcription. Not every entry has an audio file
// to key off of, so those are kept unlabeled rather than guessing a dialect
// from the IPA symbols themselves. Different dialects sometimes share the
// exact same transcription text (e.g. "screen" is spelled /skɹiːn/ in both
// its AU and US audio entries) - those are NOT duplicates and must both be
// kept, so entries are deduped on (text, label) together, never on text
// alone. Only an unlabeled entry whose text exactly matches an already-
// labeled entry is dropped, since it adds no information the labeled
// version doesn't already cover. US is always sorted first when present;
// everything else keeps its original order.
function pickPhonetics(phonetics) {
  const entries = []
  const seenKeys = new Set()

  for (const p of phonetics) {
    if (!p.text) continue
    const match = p.audio?.match(/-(\w+)\.mp3$/)
    const label = (match && REGION_LABELS[match[1]]) || ''
    const key = `${p.text}|${label}`
    if (seenKeys.has(key)) continue
    seenKeys.add(key)
    entries.push({ text: p.text, label, audio: p.audio || null })
  }

  const labeledTexts = new Set(entries.filter((e) => e.label).map((e) => e.text))
  const deduped = entries.filter((e) => e.label || !labeledTexts.has(e.text))

  return deduped.sort((a, b) => {
    if (a.label === 'US') return -1
    if (b.label === 'US') return 1
    return 0
  })
}

// Returns a normalized { word, phonetics, usPhonetics, meanings, sourceUrl,
// license } object, or null if nothing was found (unknown word, network
// error, etc).
export async function fetchWordInfo(word, lang = 'en') {
  const key = `${lang}:${word.trim().toLowerCase()}`
  if (cache.has(key)) return cache.get(key)

  let data
  try {
    const response = await fetch(`${API_BASE}/${lang}/${encodeURIComponent(word)}`)
    if (!response.ok) return null
    data = await response.json()
  } catch {
    return null
  }

  const entry = Array.isArray(data) ? data[0] : null
  if (!entry) return null

  const phonetics = pickPhonetics(entry.phonetics || [])

  const result = {
    word: entry.word,
    phonetics,
    // pickPhonetics() always sorts a US entry first when one exists, so
    // this is just "is there one at all" - kept as its own field since the
    // header display cares about exactly one thing: is there a US
    // transcription to show, or not.
    usPhonetics: phonetics[0]?.label === 'US' ? phonetics[0] : null,
    meanings: (entry.meanings || []).map((meaning) => ({
      partOfSpeech: meaning.partOfSpeech,
      definitions: (meaning.definitions || []).map((def) => ({
        definition: def.definition,
        example: def.example || null,
        synonyms: def.synonyms || [],
      })),
    })),
    sourceUrl: entry.sourceUrls?.[0] || null,
    license: entry.license?.name || null,
  }

  cache.set(key, result)
  return result
}
