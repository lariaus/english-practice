#pragma once

#include "dictionary_utils/dictionary_entry.h"
#include "dictionary_utils/free_dictionary_api_entry.h"
#include "dictionary_utils/wiktionary_api_entry.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace dictionary_utils::detail {

// Maps Wiktionary's free-form raw accent tags (e.g. "GenAm", "RP",
// "Southern England") down to the small fixed vocabulary the JS webapp
// already uses (see dictionaryClient.js's REGION_LABELS): "US", "UK", "AU",
// or "" for anything not confidently classified - matching JS's own
// philosophy of not guessing a dialect it isn't sure of. If a tag list
// contains more than one recognized accent (e.g. a pronunciation shared
// between "US,UK"), US wins, matching this class's whole "identify the US
// one" purpose.
std::string normalizeAccentLabel(const std::vector<std::string>& rawAccents);

std::vector<MeaningEntry> buildMeanings(const std::vector<Entry>& entries);

// Builds the final phonetics[] list from Wiktionary's raw IPA entries, plus
// the already-resolved relative path of the cached primary audio (empty if
// none was cached) and that same audio entry's own raw accent tags. One row
// per IPA entry, in source order; if the cached audio's accent doesn't match
// any IPA row's normalized label, an extra audio-only row is appended so a
// real recording is never silently dropped. An IPA row tagged only with a
// non-geographic qualifier (a register/dialect-merger name like "strong
// form" or "cot-caught" rather than a country) is also recognized as US when
// every one of its tags also appears on the cached US audio's own tags - the
// two are unambiguously the same pronunciation, just missing the country
// half of the tag on the IPA side specifically (real examples: "at", "than",
// "thank", "dog", "marry"). Deliberately returns the raw, unfiltered list -
// deduping exact (text, label) duplicates, capping unlabeled rows, and
// sorting US-first are all display concerns handled client-side, in
// dictionaryClient.js.
std::vector<PhoneticEntry> buildPhonetics(const std::vector<IpaPronunciation>& ipa,
                                           const std::string& cachedUsAudioRelativePath,
                                           const std::vector<std::string>& cachedUsAudioAccents);

// The fastFetch phonetics source: FreeDictionaryAPIEntry's own pronunciation
// data (text + accent tags, e.g. "General American") - never any audio,
// since only Wiktionary ever has real recordings. Same raw/unfiltered
// contract as buildPhonetics above, just fed from a different, weaker
// source.
std::vector<PhoneticEntry> buildPhoneticsFromFreeDictionary(const std::vector<Entry>& entries);

// Combines a list of (possibly partial) per-source DictionaryEntry
// contributions into one. For word/meanings/sourceUrl/license: takes the
// value from the first entry in the list that has non-empty data for that
// field - once a field is filled, later entries never overwrite it, even
// with different non-empty data of their own. Generic over any number of
// sources in any order; the caller chooses list order to encode priority
// (e.g. `word` prefers whichever source is listed first).
//
// `phonetics` is the one exception, deliberately content-aware rather than
// list-order-based: prefers whichever entry's phonetics has both audio and
// IPA text, then audio-only, then IPA-only, regardless of where that entry
// sits in the list. A plain first-non-empty-wins rule (like every other
// field) would make "never discard real audio in favor of a plain
// IPA-only contribution" depend entirely on the audio-having source always
// being listed first - true of Wiktionary in DictionaryEntry::fetch()
// today, but not something this function itself would notice or protect
// if that ordering ever changed, or a future third phonetics-contributing
// source landed ahead of it in the list.
//
// If no source contributed a `word` either, falls back to `fallbackWord`
// (the original queried word) instead of leaving it empty.
DictionaryEntry mergeDictionaryEntries(const std::vector<DictionaryEntry>& entries,
                                        const std::string& fallbackWord);

// Looks up one source (SourceClass is FreeDictionaryAPIEntry or
// WiktionaryAPIEntry - anything exposing a matching static fetch()/name())
// and, if found, converts and appends its DictionaryEntry contribution to
// `entries` - so a caller can collect every source's contribution into one
// vector before calling mergeDictionaryEntries above.
//
// Owns this source's whole try/catch: any exception from SourceClass::fetch
// is caught and rethrown as DictionaryEntryError, labeled with
// SourceClass::name() - the word genuinely not being found is not an error
// (SourceClass::fetch signals that via nullopt, not an exception), so
// reaching the catch here always means a real failure (network/parse/
// unexpected status), never just "not found".
template <typename SourceClass>
void fetchEntryOf(std::vector<DictionaryEntry>& entries, const std::string& normalizedWord,
                  const std::filesystem::path& serverDataDir, const std::string& languageCode,
                  bool ignoreCache) {
  try {
    auto raw = SourceClass::fetch(normalizedWord, serverDataDir, languageCode, ignoreCache);
    if (!raw) return;
    entries.push_back(dictionaryEntryFrom(*raw));
  } catch (const std::exception& e) {
    throw DictionaryEntryError(SourceClass::name() + " lookup failed for \"" + normalizedWord +
                                "\": " + e.what());
  }
}

// Same idea as fetchEntryOf above, but for DictionaryEntry::fetchFromCache()
// instead of fetch() - calls SourceClass::fetchFromCache() (never
// SourceClass::fetch()), so this never touches the network under any
// circumstance and never throws (fetchFromCache() itself has nowhere to
// propagate a failure to - a missing or corrupt cache file both just mean
// "nothing usable here", not an error). No `ignoreCache` parameter, unlike
// fetchEntryOf - meaningless for a cache-only lookup.
template <typename SourceClass>
void fetchCachedEntryOf(std::vector<DictionaryEntry>& entries, const std::string& normalizedWord,
                         const std::filesystem::path& serverDataDir,
                         const std::string& languageCode) {
  auto raw = SourceClass::fetchFromCache(normalizedWord, serverDataDir, languageCode);
  if (!raw) return;
  entries.push_back(dictionaryEntryFrom(*raw));
}

// The ServerData-relative path a successful lookup's whole-entry cache file
// lives at, e.g. "dictionaries/entries/en-smart.json" - a sibling of
// FreeDictionaryAPIEntry's own cache (dictionaries/freedictionaryapi/) and
// WiktionaryAPIEntry's own cache (dictionaries/wiktionaryapi/), each layer
// caching its own shape independently. The actual downloaded audio files
// live under dictionaries/wiktionaryapi/ too (it's Wiktionary's own data,
// fetched only by that class) - this entries/ cache just references that
// same path rather than copying the bytes anywhere. Both languageCode and
// word are percent-encoded into the filename so neither can inject a path
// separator or otherwise produce an unsafe/ambiguous filename.
std::string entryCacheRelativePath(const std::string& languageCode, const std::string& word);

// Pure (de)serialization of a DictionaryEntry to/from its cache-file JSON
// shape - independent of dictionary_route.cpp's own JSON building in
// native_server_core (a different library/layer, can't share this directly;
// duplication here is intentional, matching this codebase's established
// per-layer convention).
nlohmann::json dictionaryEntryToJson(const DictionaryEntry& entry);

// Throws nlohmann::json::exception on malformed JSON - callers should treat
// that as a cache miss (fall through to a live lookup) rather than a hard
// failure, since a corrupted cache file shouldn't break a lookup that would
// otherwise succeed live.
DictionaryEntry dictionaryEntryFromJson(const nlohmann::json& data);

}  // namespace dictionary_utils::detail
