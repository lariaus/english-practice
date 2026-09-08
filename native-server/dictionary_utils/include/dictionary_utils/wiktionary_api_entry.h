#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dictionary_utils {

// Wiktionary tags US pronunciation inconsistently across its own templates -
// "US" is common on {{audio|...}}, "GenAm"/"GA" (General American) is common
// on {{IPA|...}}, even for the exact same word/entry (e.g. "smart" uses
// "GenAm", "endurance" uses "GA", both on the same word as an "US"-tagged
// {{audio}} line). Checks every spelling actually seen in practice so
// callers don't have to know this.
bool isUsAccentTag(const std::string& tag);
bool hasUsAccent(const std::vector<std::string>& accents);

struct AudioPronunciation {
  std::string filename;  // as referenced in wikitext, e.g. "en-us-smart.ogg" (no "File:" prefix)
  std::string audio;      // relative path under ServerData, e.g. "dictionaries/wiktionaryapi/en-us-smart.ogg" -
                           // empty unless this is the one actually downloaded (only ever the
                           // first US-tagged entry - see WiktionaryAPIEntry::fetch)
  std::string mimeType;    // empty unless downloaded
  double durationSeconds = 0.0;  // 0 unless downloaded
  long long sizeBytes = 0;       // 0 unless downloaded
  // Raw, free-form tags exactly as they appear in wikitext (e.g. "US",
  // "GenAm", "Southern England") - NOT normalized, and may be empty if the
  // source template had no accent qualifier at all. Use isUsAudioCandidate()
  // (not hasUsAccent() directly) to check an AudioPronunciation for US -
  // some US recordings are only tagged with a specific US dialect (e.g.
  // "California") rather than "US"/"GenAm"/"GA", which hasUsAccent() alone
  // won't recognize.
  std::vector<std::string> accents;
};

// Whether this audio candidate is a US recording - true if its own accent
// tags say so (hasUsAccent), OR its filename follows Wiktionary's own
// "en-us-..." naming convention for US recordings. The filename check
// exists because Wiktionary sometimes tags a US recording with a specific
// US dialect name instead (e.g. "California" on "en-us-talent.ogg") that
// hasUsAccent() has no way to recognize as US on its own.
bool isUsAudioCandidate(const AudioPronunciation& audio);

struct IpaPronunciation {
  std::string text;  // e.g. "/smɑɹt/"
  std::vector<std::string> accents;
};

struct WiktionaryWordInfo {
  std::string word;
  std::vector<AudioPronunciation> audio;
  std::vector<IpaPronunciation> ipa;

  // True only if the word (i.e. its language section) has a Pronunciation
  // subsection at all - a word can be found (see fetch() below) but have
  // no Pronunciation subsection (rare but real).
  bool hasPronunciationSection = false;

  // First audio/IPA pronunciation tagged as US, or nullptr if none -
  // pointers into `audio`/`ipa`, valid as long as this WiktionaryWordInfo
  // is alive.
  const AudioPronunciation* usAudio() const;
  const IpaPronunciation* usIpa() const;
};

// Thrown only for actual failures (network/transport error, unexpected HTTP
// status, unparseable response, or an unexpected Wiktionary API error) -
// never for a word/language simply not being found, see fetch() below.
class WiktionaryApiError : public std::runtime_error {
 public:
  explicit WiktionaryApiError(const std::string& message) : std::runtime_error(message) {}
};

// Looks up one word's real recorded pronunciation audio and IPA text
// directly from Wiktionary's own official MediaWiki API - see
// docs/dictionary-spec.md for why this exists alongside FreeDictionaryAPIEntry
// (that one has real structured definitions but no audio at all).
//
// Deliberately narrow in scope: only pronunciation (audio + IPA), not
// definitions/senses/forms - FreeDictionaryAPIEntry already covers those.
//
// Purely static, mirroring FreeDictionaryAPIEntry's own shape - no
// per-lookup instance state worth keeping between a cache check and a live
// fetch.
class WiktionaryAPIEntry {
 public:
  // A human-readable label for this source, used in DictionaryEntry's own
  // error messages when wrapping a failure from this class - not
  // meaningful for anything else.
  static std::string name() { return "Wiktionary"; }

  // A short tag for this source, for callers that want a compact label
  // instead of name()'s full one - e.g. dictionary-crawler's summary output.
  static std::string getTagName() { return "WDic"; }

  // Checks a persistent on-disk cache under ServerData's
  // `dictionaries/wiktionaryapi/` (keyed by languageCode+word, a sibling of
  // - and deliberately separate from - DictionaryEntry's own whole-entry
  // cache and FreeDictionaryAPIEntry's own cache) before hitting the
  // network. On a cache miss: looks up the word's Pronunciation section,
  // and - if a primary (US-preferred) audio recording is found - downloads
  // it directly into that same `dictionaries/wiktionaryapi/` directory
  // (it's Wiktionary's own data, fetched only by this class), storing the
  // resulting relative path on that entry's `audio` field.
  // DictionaryEntry just references this same path afterward rather than
  // copying the bytes anywhere. Non-primary audio entries are listed
  // (filename + accents, from wikitext alone) but never resolved or
  // downloaded - their `audio`/`mimeType`/`durationSeconds`/`sizeBytes`
  // stay empty/zero.
  //
  // `languageCode` uses the same convention as FreeDictionaryAPIEntry (e.g.
  // "en") - mapped internally to Wiktionary's own full-language-name
  // section heading (e.g. "English"), since Wiktionary pages are organized
  // by full language name headings rather than codes (one page covers a
  // word in every language that uses it).
  //
  // Returns nullopt if the page doesn't exist at all, or exists but has no
  // section for the requested language (e.g. a German-only entry when
  // asking for English) - a normal, expected outcome, not an error. A
  // word that *is* found but has no Pronunciation subsection at all (rare
  // but real) still returns a value, just with `hasPronunciationSection`
  // false and empty `audio`/`ipa`. A not-found result is never written to
  // the cache (nothing to persist - a repeat lookup for the same word not
  // on Wiktionary just hits the network again), so a cache file existing
  // always means the word was found.
  //
  // Throws WiktionaryApiError on an actual failure (network/parse/
  // unexpected status/unexpected Wiktionary API error) - including when a
  // primary audio candidate is found but actually fetching it fails (bad
  // imageinfo response, failed GET, non-200, disk write failure). Audio is
  // the whole reason this class exists over FreeDictionaryAPIEntry, so a
  // candidate found but not actually downloadable is a real failure of
  // this lookup, not something to paper over - unlike a word genuinely
  // having no US-tagged candidate at all, which is not an error. A thrown
  // failure here also means nothing gets written to this class's own
  // cache for that lookup, so a later retry does a real fetch again
  // rather than replaying the same failure forever.
  static std::optional<WiktionaryWordInfo> fetch(const std::string& word,
                                                  const std::filesystem::path& serverDataDir,
                                                  const std::string& languageCode = "en",
                                                  bool ignoreCache = false);

  // Reads (and parses) this word's cache file, if any - never touches the
  // network under any circumstance. Returns nullopt for a word that's never
  // been fetched, one whose cache file is corrupt, and one that genuinely
  // has no Pronunciation section (a found-but-empty result would still
  // parse fine here and come back non-nullopt, same as fetch() itself) -
  // only a missing/unparseable cache file returns nullopt. `word` is used
  // as-is (not normalized) - same convention as fetch() itself.
  static std::optional<WiktionaryWordInfo> fetchFromCache(const std::string& word,
                                                           const std::filesystem::path& serverDataDir,
                                                           const std::string& languageCode = "en");

  // Whether this word already has a cache file under
  // `dictionaries/wiktionaryapi/`, without reading or parsing it - a cheap
  // existence check for callers that want to skip a word already fetched
  // (e.g. a batch scraper resuming a partial run) without paying for a live
  // fetch or even a cache read. Since fetch() never caches a not-found
  // result, a cache file existing always means the word was found - a
  // not-found word is never reported as cached here.
  static bool hasCachedEntry(const std::string& word, const std::filesystem::path& serverDataDir,
                              const std::string& languageCode = "en");
};

}  // namespace dictionary_utils
