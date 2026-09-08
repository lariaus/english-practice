#pragma once

#include "dictionary_utils/wiktionary_api_entry.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dictionary_utils::detail {

// One entry from action=parse&prop=tocdata's section list, flattened to
// just the fields the lookup below needs.
struct TocSection {
  int tocLevel = 0;
  std::string line;
  std::string index;
};

// Pure parse of a tocdata API response - no network, so tests can feed it
// fixture JSON directly.
std::vector<TocSection> parseTocSections(const nlohmann::json& tocdataResponse);

// Maps a languageCode (e.g. "en") to the exact top-level heading Wiktionary
// uses for that language (e.g. "English") - Wiktionary pages are organized
// by full language name headings, not codes, since one page covers a word
// in every language that uses it. Only English is actually used anywhere
// in this app today - extend this if/when another language is needed.
std::string languageSectionForCode(const std::string& languageCode);

struct PronunciationSectionLookup {
  bool languageFound = false;
  bool pronunciationFound = false;  // true iff sectionIndices is non-empty
  std::string sectionIndex;         // sectionIndices.front() - kept for convenience
  // Every Pronunciation-like heading nested under the language heading, in
  // page order: a plain "Pronunciation", or - when a word has multiple
  // senses with different pronunciations (e.g. "interrupt": the verb and
  // the noun stress differently) - each numbered "Pronunciation 1",
  // "Pronunciation 2", etc. Usually has zero or one entries; more than one
  // only when the page actually splits pronunciations this way.
  std::vector<std::string> sectionIndices;
};

// Finds every Pronunciation-like subsection nested under the given
// top-level language heading (e.g. "English") - stops looking once it hits
// the next top-level (tocLevel == 1) section, so a Pronunciation subsection
// under a *different* language never gets mistaken for this one's.
PronunciationSectionLookup findPronunciationSection(const std::vector<TocSection>& sections,
                                                     const std::string& languageSection);

struct ParsedPronunciationWikitext {
  // audio/mimeType/durationSeconds/sizeBytes are left unset here - resolved
  // separately for at most the one primary entry, since that requires a
  // second (and third, for the actual bytes) network round-trip this pure
  // function doesn't make.
  std::vector<AudioPronunciation> audio;
  std::vector<IpaPronunciation> ipa;
};

// Parses {{audio|...}} and {{IPA|...}} templates out of a Pronunciation
// section's raw wikitext. Pure text parsing, no network. Correctly matches
// a template even when another template is nested inside one of its own
// parameters (e.g. a citation reference) - see findTopLevelTemplates() in
// the .cpp. A parameter value containing a literal `|` (e.g. inside a
// `[[...|...]]` wikilink) can still be mis-split into extra parameters,
// though - not exercised by anything read from the result today.
ParsedPronunciationWikitext parsePronunciationWikitext(const std::string& wikitext);

// Whether a Pronunciation section's parsed content is good enough to use
// without looking at another candidate section: a real IPA transcription,
// and a US-recognized audio recording (see isUsAudioCandidate()).
bool hasIpaAndUsAudio(const ParsedPronunciationWikitext& parsed);

// Given each candidate Pronunciation-like section's already-parsed content,
// in page order (e.g. "Pronunciation 1", "Pronunciation 2", ... for a word
// that splits pronunciation by sense - see PronunciationSectionLookup),
// picks which one to use: the first with hasIpaAndUsAudio(), or - if none
// qualify - the very first section regardless, since it has precedence and
// there's nothing better to fall back to. `candidates` may be a prefix of
// every section that exists on the page - the caller can stop fetching
// further ones early, as soon as hasIpaAndUsAudio() is satisfied, and pass
// just what it fetched so far.
ParsedPronunciationWikitext selectPronunciationSection(
    const std::vector<ParsedPronunciationWikitext>& candidates);

// Wiktionary sometimes has *multiple* real US-tagged audio candidates for
// the same word, each covering a different regional/phonological
// sub-variant (real example, "dog": "En-us-ne-dog.ogg" tagged plain "US",
// and "en-us-dog.ogg" tagged "US, cot-caught"). Only one is ever downloaded
// as the primary recording - prefer whichever one shares a qualifier tag
// with an existing IPA line (so buildPhonetics's own qualifier-subset rule
// downstream has something real to match against), falling back to the
// first US-tagged candidate in page order when none do. Returns nullptr if
// there's no US-tagged candidate at all. Pure selection logic, no I/O -
// `audio` is mutated only in the sense that the caller gets back a pointer
// into it to actually download.
AudioPronunciation* selectPrimaryUsAudio(std::vector<AudioPronunciation>& audio,
                                          const std::vector<IpaPronunciation>& ipa);

struct ResolvedAudioMetadata {
  std::string url;
  std::string mimeType;
  double durationSeconds = 0.0;
  long long sizeBytes = 0;
};

// Extracts the resolved metadata (including the actual download URL) from a
// single-title action=query&prop=imageinfo response - nullopt if the
// response has no resolved imageinfo at all (e.g. a title-normalization
// mismatch), which should degrade gracefully rather than erroring. Only
// ever called with a single-title query (the primary audio candidate), so
// unlike the old batched approach, no filename-matching is needed - just
// takes whatever one resolved entry the response has.
std::optional<ResolvedAudioMetadata> resolveAudioMetadata(const nlohmann::json& imageinfoResponse);

// Pure decision: should this specific audio file actually be (re)downloaded?
// No I/O - the caller does the actual exists() check and network call. A
// file already present on disk might belong to a different word that
// happens to share the same Commons recording, so this check still matters
// even on a live (cache-miss) fetch.
bool shouldDownloadAudio(bool alreadyCached, bool ignoreCache);

// Pure (de)serialization of a WiktionaryWordInfo to/from its cache-file
// JSON shape.
nlohmann::json wiktionaryWordInfoToJson(const WiktionaryWordInfo& info);

// Throws nlohmann::json::exception on malformed JSON - callers should treat
// that as a cache miss (fall through to a live lookup) rather than a hard
// failure.
WiktionaryWordInfo wiktionaryWordInfoFromJson(const nlohmann::json& data);

}  // namespace dictionary_utils::detail
