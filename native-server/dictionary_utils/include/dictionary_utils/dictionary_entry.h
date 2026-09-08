#pragma once

#include "dictionary_utils/free_dictionary_api_entry.h"
#include "dictionary_utils/wiktionary_api_entry.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dictionary_utils {

struct DefinitionEntry {
  std::string definition;
  std::vector<std::string> examples;  // superset of the current JS shape's singular `example`
  std::vector<std::string> synonyms;
  std::vector<std::string> antonyms;  // not in the current JS shape at all - available if it wants it
};

struct MeaningEntry {
  std::string partOfSpeech;
  std::vector<DefinitionEntry> definitions;
};

struct PhoneticEntry {
  std::string text;   // IPA text - may be empty (an audio-only row, see DictionaryEntry::fetch)
  std::string label;  // normalized: "US", "UK", "AU", or "" - never a raw Wiktionary tag like "GenAm"
  std::string audio;  // relative path under ServerData, e.g.
                       // "dictionaries/wiktionaryapi/en-us-smart.ogg"; empty for every row except
                       // the cached primary (US-preferred) pronunciation
};

class DictionaryEntryError : public std::runtime_error {
 public:
  explicit DictionaryEntryError(const std::string& message) : std::runtime_error(message) {}
};

// Combines FreeDictionaryAPIEntry (definitions/meanings) and
// WiktionaryAPIEntry (real pronunciation audio + IPA) into the shape the JS
// webapp's dictionary popup expects - see dictionaryClient.js's
// fetchWordInfo(). A deliberate superset where the underlying sources have
// richer data than today's JS UI uses (multiple examples, antonyms).
//
// Unlike FreeDictionaryAPIEntry/WiktionaryAPIEntry, this is the data itself
// (fields directly on the object) rather than a handle class with a
// separately-named result struct - matching the JS shape's own flatness.
class DictionaryEntry {
 public:
  std::string word;
  std::vector<PhoneticEntry> phonetics;
  std::vector<MeaningEntry> meanings;
  std::string sourceUrl;
  std::string license;

  // First phonetics row labeled "US", or nullptr if none - a computed
  // accessor (not a stored pointer) so it stays valid across copies/moves,
  // same pattern as WiktionaryWordInfo::usAudio()/usIpa().
  const PhoneticEntry* usPhonetics() const;

  // Whether any "US"-labeled phonetics row carries real IPA text / a real
  // audio recording, respectively - checks every row (not just the first,
  // unlike usPhonetics()) since a US IPA row and a US audio-only row can be
  // separate entries in `phonetics` (see buildPhonetics()'s own doc
  // comment). FreeDictionaryAPIEntry never contributes audio at all, so
  // hasEnUsAudio() is only meaningful for a Wiktionary-sourced entry.
  bool hasEnUsIPA() const;
  bool hasEnUsAudio() const;

  // Trims surrounding whitespace and lowercases - the exact normalization
  // fetch() below applies to `word` before querying either source. Exposed
  // so other callers that talk to FreeDictionaryAPIEntry/WiktionaryAPIEntry
  // directly (which do no normalization of their own) can match fetch()'s
  // own behavior when they want it.
  static std::string normalizeWord(const std::string& word);

  // Reads (and parses) the whole-entry cache fetch() itself writes to and
  // checks first - normalizes `word` the same way fetch() does. Never
  // touches the network under any circumstance: returns nullopt for a word
  // that's never been resolved before, one whose cache file is corrupt/
  // unparseable, and one that genuinely has no entry alike - all three look
  // identical from a pure cache-read's perspective, which isn't this
  // function's concern. fetch() itself calls this first and returns
  // whatever it finds without going further - exposed publicly so a caller
  // that specifically wants "is this already resolved, with zero network
  // risk" (e.g. ShadowLoopMode's bulk vocabulary filter - see
  // docs/shadow-loop-mode-spec.md) can ask the exact same question fetch()
  // itself would, without a separate, parallel cache-reading implementation.
  static std::optional<DictionaryEntry> fetchFromCache(const std::string& word,
                                                        const std::filesystem::path& serverDataDir,
                                                        const std::string& languageCode = "en");

  // Looks up `word` against both sources and merges them. `serverDataDir`
  // (the actual `<dataDir>/server_data` filesystem path - the caller
  // resolves this; DictionaryEntry doesn't know about CLI config) is
  // threaded through to both sources' own caches - WiktionaryAPIEntry owns
  // the actual audio-download step itself (see WiktionaryAPIEntry::fetch),
  // so this class just references whatever local path it already
  // downloaded to, never copying the bytes anywhere. `word` is trimmed and
  // lowercased before lookup - a deliberate convenience for callers passing
  // raw clicked/typed input, at the cost of genuinely case-sensitive
  // Wiktionary entries (e.g. "May" the month vs "may" the verb) not being
  // reachable through this class.
  //
  // Returns nullopt only if the word genuinely isn't found in either
  // source (each source represents "not found" as a normal return value -
  // WordInfo::found()/WiktionaryWordInfo::found - never an exception, so
  // this is a routine outcome, not a failure). The two sources are treated
  // symmetrically on real failure: a genuine error from either
  // FreeDictionaryAPIEntry or WiktionaryAPIEntry (network, parse,
  // unexpected status - never just "not found") always throws
  // DictionaryEntryError, even if the other source succeeded. Neither
  // source's real failure is silently absorbed in favor of the other's
  // partial data.
  //
  // A successful result is also cached whole, as JSON, under ServerData's
  // `dictionaries/entries/` - keyed by language + word - so a repeat lookup
  // for the same word is a pure disk read with zero API calls, unless
  // `ignoreCache` is true. A word that resolved to nullopt (genuinely not
  // found) is never cached, since that's cheap to re-check and re-checking
  // might find it later.
  //
  // `ignoreCache` is a single switch controlling every layer of caching
  // this pulls in - deliberately not split into separate per-layer flags
  // (that got confusing fast): the whole-entry cache above,
  // WiktionaryAPIEntry's own cache (including its downloaded audio file),
  // and FreeDictionaryAPIEntry's own persistent cache all get bypassed
  // together when true, and the fresh result refreshes all of them
  // afterward - so `ignoreCache=true` doubles as "force a full refresh."
  //
  // If `fastFetch` is true, `ignoreCache` is ignored entirely (not composed
  // with it - simply not consulted, at every layer). The entry cache is
  // still always checked first and returned as-is if present, whether it
  // was originally populated by a fast or full lookup. On a cache miss,
  // only FreeDictionaryAPIEntry is queried (via its own cached wrapper,
  // always allowed to use its cache in this mode) - WiktionaryAPIEntry (and
  // therefore any audio download) is skipped entirely, minimizing API calls
  // at the cost of no real pronunciation audio. `phonetics` in that case
  // comes from FreeDictionaryAPIEntry's own (audio-less) pronunciation data
  // instead of being left empty. A fastFetch-produced result is never
  // written to the whole-entry cache, so a later non-fast lookup for the
  // same word still does the real full lookup rather than being satisfied
  // by this degraded one. A FreeDictionaryAPIEntry failure during fastFetch
  // throws DictionaryEntryError (there's no second source to fall back on
  // in this mode, so any failure to check is a real failure, not a "not
  // found").
  static std::optional<DictionaryEntry> fetch(const std::string& word,
                                               const std::filesystem::path& serverDataDir,
                                               const std::string& languageCode = "en",
                                               bool fastFetch = false, bool ignoreCache = false);
};

// Converts one source's own raw result into the common DictionaryEntry
// shape, reflecting only what that source itself knows - fields it has no
// data for stay empty/default. No knowledge of any other source, no
// word-fallback-to-the-queried-word logic, no merging or caching - a
// caller that wants DictionaryEntry::fetch()'s full combine-both-sources-
// and-cache behavior should call that instead. These are for a caller that
// specifically wants one source's own contribution on its own merits.
// Overloaded on the raw source type rather than separately named.
DictionaryEntry dictionaryEntryFrom(const WordInfo& freeDict);

// Same idea for Wiktionary - resolves the cached primary (US) audio path
// itself via `wikt.usAudio()` rather than taking it as a separate
// parameter, since that's derivable purely from `wikt`.
DictionaryEntry dictionaryEntryFrom(const WiktionaryWordInfo& wikt);

}  // namespace dictionary_utils
