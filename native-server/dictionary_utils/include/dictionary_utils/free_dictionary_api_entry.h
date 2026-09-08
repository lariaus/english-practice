#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dictionary_utils {

struct Pronunciation {
  std::string type;  // e.g. "ipa"
  std::string text;
  std::vector<std::string> tags;  // e.g. "General American", "Received Pronunciation"
};

struct WordForm {
  std::string word;
  std::vector<std::string> tags;  // e.g. "comparative", "plural"
};

struct Quote {
  std::string text;
  std::string reference;
};

// Recursive to mirror the API's own `subsenses` nesting - in practice this
// is empty for almost every word (see docs/dictionary-spec.md), but modeled
// faithfully rather than flattened away.
struct Sense {
  std::string definition;
  std::vector<std::string> tags;
  std::vector<std::string> examples;
  std::vector<Quote> quotes;
  std::vector<std::string> synonyms;
  std::vector<std::string> antonyms;
  std::vector<Sense> subsenses;
};

struct Language {
  std::string code;
  std::string name;
};

// One part-of-speech entry for the word - e.g. "run" has separate verb/noun/
// adjective entries, each with its own pronunciations, forms, and senses.
struct Entry {
  Language language;
  std::string partOfSpeech;
  std::vector<Pronunciation> pronunciations;
  std::vector<WordForm> forms;
  std::vector<Sense> senses;
  std::vector<std::string> synonyms;
  std::vector<std::string> antonyms;
};

struct License {
  std::string name;
  std::string url;
};

struct Source {
  std::string url;
  License license;
};

// One word lookup's full structured result - mirrors freedictionaryapi.com's
// response shape one-to-one rather than collapsing it down early.
struct WordInfo {
  std::string word;
  std::vector<Entry> entries;
  Source source;
};

// Thrown only for actual failures (network/transport error, unexpected HTTP
// status, unparseable response) - never for a word simply not being found,
// see fetch() below.
class FreeDictionaryApiError : public std::runtime_error {
 public:
  explicit FreeDictionaryApiError(const std::string& message) : std::runtime_error(message) {}
};

// Looks up one word against freedictionaryapi.com - a keyless,
// Wiktionary-backed dictionary API. Note: this does NOT provide audio
// pronunciation URLs (the API doesn't expose any) - only IPA text tagged by
// accent (e.g. "General American" for US). Real pronunciation audio, when
// wired up, comes from a separate source (Wiktionary's own API) - see
// docs/dictionary-spec.md.
//
// Purely static - there's no per-lookup state worth holding onto between a
// cache check and a live fetch, so there's nothing to construct an instance
// for.
class FreeDictionaryAPIEntry {
 public:
  // A human-readable label for this source, used in DictionaryEntry's own
  // error messages when wrapping a failure from this class - not
  // meaningful for anything else.
  static std::string name() { return "FreeDictionaryAPIEntry"; }

  // A short tag for this source, for callers that want a compact label
  // instead of name()'s full one - e.g. dictionary-crawler's summary output.
  static std::string getTagName() { return "FreeDic"; }

  // Checks a persistent on-disk cache under ServerData's
  // `dictionaries/freedictionaryapi/` (keyed by language+word, a sibling of
  // - and deliberately separate from - DictionaryEntry's own whole-entry
  // cache under `dictionaries/` directly; some duplicated data between the
  // two is an accepted tradeoff, since these are small files) before
  // hitting the network, and writes a fresh live result back to it unless
  // `ignoreCache` is true. Caches the raw upstream JSON verbatim rather
  // than a custom re-serialization, so a cached read and a live fetch
  // always parse through the exact same logic - no separate schema to keep
  // in sync or drift out of.
  //
  // Returns nullopt if the word genuinely isn't in the dictionary - the
  // API itself returns HTTP 200 with an empty `entries` array for that, not
  // an error status, so this is a normal, expected outcome, not a failure.
  // A not-found result is never written to the cache (nothing to persist -
  // a repeat lookup for the same nonexistent word just hits the network
  // again), so a cache file existing always means the word was found.
  // Throws FreeDictionaryApiError on an actual failure (network/parse/
  // unexpected status) - never for a word simply not being found.
  static std::optional<WordInfo> fetch(const std::string& word,
                                        const std::filesystem::path& serverDataDir,
                                        const std::string& languageCode = "en",
                                        bool ignoreCache = false);

  // Reads (and parses) this word's cache file, if any - never touches the
  // network under any circumstance. Returns nullopt for a word that's never
  // been fetched, one whose cache file is corrupt, and one that genuinely
  // has no entry alike - indistinguishable from a pure cache-read's
  // perspective. `word` is used as-is (not normalized) - same convention as
  // fetch() itself.
  static std::optional<WordInfo> fetchFromCache(const std::string& word,
                                                 const std::filesystem::path& serverDataDir,
                                                 const std::string& languageCode = "en");

  // Whether this word already has a cache file under
  // `dictionaries/freedictionaryapi/`, without reading or parsing it - a
  // cheap existence check for callers that want to skip a word already
  // fetched (e.g. a batch scraper resuming a partial run) without paying
  // for a live fetch or even a cache read. Since fetch() never caches a
  // not-found result, a cache file existing always means the word was
  // found - a not-found word is never reported as cached here.
  static bool hasCachedEntry(const std::string& word, const std::filesystem::path& serverDataDir,
                              const std::string& languageCode = "en");
};

}  // namespace dictionary_utils
