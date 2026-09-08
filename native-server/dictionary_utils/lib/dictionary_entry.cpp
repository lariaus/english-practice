#include "dictionary_utils/dictionary_entry.h"

#include "dictionary_entry_detail.h"

#include "dictionary_utils/free_dictionary_api_entry.h"
#include "dictionary_utils/wiktionary_api_entry.h"

#include <nlohmann/json.hpp>
#include <server_data/server_data_store.h>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace dictionary_utils {

namespace {

// Words whose only real English dictionary entry is capitalized (a proper
// noun), but that would otherwise normalize to an all-lowercase spelling
// with no English entry at all - e.g. Wiktionary's lowercase "september"
// page exists but only covers Danish/Dutch/etc. (languages that don't
// capitalize month names); the English entry lives at "September"
// specifically, since Wiktionary is fully case-sensitive (not just
// first-letter, unlike Wikipedia).
//
// Scoped deliberately to calendar terms (months, weekdays, named holidays)
// rather than proper nouns in general (personal names, places) - this
// mechanism has no way to tell context, so it always capitalizes a listed
// word on every lookup; that's a fine trade for a word with no plausible
// lowercase meaning at all, but would silently break a common word with a
// real lowercase meaning of its own. That's exactly why "may" and "march"
// (modal verb / "march forward") and "august" (the adjective) are
// deliberately left out despite being calendar terms - add them only if
// their lowercase everyday meaning stops mattering more than the month.
// Add anything else to this set only when a lowercase spelling is
// confirmed to have no English section at all.
const std::unordered_set<std::string>& capitalizedWords() {
  static const std::unordered_set<std::string> words = {
      "september", "october",  "november",  "december", "january", "february",
      "april",     "june",     "july",      "monday",   "tuesday", "wednesday",
      "thursday",  "friday",   "saturday",  "sunday",   "christmas",
  };
  return words;
}

}  // namespace

std::string DictionaryEntry::normalizeWord(const std::string& word) {
  size_t b = word.find_first_not_of(" \t\n\r");
  if (b == std::string::npos) return "";
  size_t e = word.find_last_not_of(" \t\n\r");
  std::string trimmed = word.substr(b, e - b + 1);
  std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(),
                  [](unsigned char c) { return std::tolower(c); });

  if (capitalizedWords().count(trimmed)) {
    trimmed[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(trimmed[0])));
  }

  return trimmed;
}

const PhoneticEntry* DictionaryEntry::usPhonetics() const {
  for (const auto& p : phonetics) {
    if (p.label == "US") return &p;
  }
  return nullptr;
}

bool DictionaryEntry::hasEnUsIPA() const {
  for (const auto& p : phonetics) {
    if (p.label == "US" && !p.text.empty()) return true;
  }
  return false;
}

bool DictionaryEntry::hasEnUsAudio() const {
  for (const auto& p : phonetics) {
    if (p.label == "US" && !p.audio.empty()) return true;
  }
  return false;
}

std::optional<DictionaryEntry> DictionaryEntry::fetchFromCache(
    const std::string& word, const std::filesystem::path& serverDataDir,
    const std::string& languageCode) {
  std::string normalizedWord = normalizeWord(word);
  server_data::ServerDataStore store(serverDataDir);
  std::string entryCachePath = detail::entryCacheRelativePath(languageCode, normalizedWord);

  auto cached = store.read(entryCachePath);
  if (cached) {
    try {
      return detail::dictionaryEntryFromJson(nlohmann::json::parse(*cached));
    } catch (const nlohmann::json::exception&) {
      // Corrupt/unreadable cache file - fall through to reassembling from
      // each source's own per-source cache below, same as a plain miss.
    }
  }

  // The fused cache doesn't have it - a totally normal case for a word
  // that's only ever been crawled (dictionary-crawler deliberately
  // populates only each source's own per-source cache, never this fused
  // one - see its own file header comment). Try reassembling purely from
  // whatever each source already has cached, same order/merge rule as
  // fetch()'s own live path, still never touching the network.
  std::vector<DictionaryEntry> entries;

  detail::fetchCachedEntryOf<WiktionaryAPIEntry>(entries, normalizedWord, serverDataDir, languageCode);
  detail::fetchCachedEntryOf<FreeDictionaryAPIEntry>(entries, normalizedWord, serverDataDir,
                                                      languageCode);

  if (entries.empty()) return std::nullopt;

  DictionaryEntry entry = detail::mergeDictionaryEntries(entries, normalizedWord);

  // Worth persisting - future calls (including fetch() itself) get this as
  // a direct fused-cache hit instead of redoing this same reassembly.
  try {
    store.write(entryCachePath, detail::dictionaryEntryToJson(entry).dump());
  } catch (const server_data::ServerDataError&) {
    // Best-effort - a cache write failure shouldn't fail an otherwise-good
    // lookup the caller is about to receive anyway.
  }

  return entry;
}

std::optional<DictionaryEntry> DictionaryEntry::fetch(const std::string& word,
                                                        const std::filesystem::path& serverDataDir,
                                                        const std::string& languageCode,
                                                        bool fastFetch, bool ignoreCache) {
  std::string normalizedWord = normalizeWord(word);

  server_data::ServerDataStore store(serverDataDir);
  std::string entryCachePath = detail::entryCacheRelativePath(languageCode, normalizedWord);

  // Check if the entry was already cached.
  if (!ignoreCache) {
    auto cached = fetchFromCache(word, serverDataDir, languageCode);
    if (cached) return cached;
  }

  if (fastFetch) {
    std::vector<DictionaryEntry> entries;
    // fastFetch always allows FreeDictionaryAPIEntry's own cache too,
    // regardless of ignoreCache - same "ignored entirely" rule as above.
    // No second source to fall back on in this mode - a failure here (from
    // inside fetchEntryOf) is a real failure, not a "not found".
    detail::fetchEntryOf<FreeDictionaryAPIEntry>(entries, normalizedWord, serverDataDir, languageCode,
                                                  /*ignoreCache=*/false);
    if (entries.empty()) return std::nullopt;
    // Deliberately never written to the cache - see fetch()'s doc comment.
    return detail::mergeDictionaryEntries(entries, normalizedWord);
  }

  std::vector<DictionaryEntry> entries;

  // WiktionaryAPIEntry now owns the whole audio-download step itself (its
  // own cache, its own file-exists check) - DictionaryEntry just references
  // whatever local path it already downloaded to, no copying. Fetched (and,
  // if found, pushed) first so it's listed first for the merge below -
  // real pronunciation audio is the whole point of consulting Wiktionary at
  // all, so its phonetics should win over FreeDictionaryAPIEntry's weaker
  // (audio-less) data whenever Wiktionary has any.
  //
  // A real failure fetching it (as opposed to the word genuinely having no
  // US audio, which WiktionaryAPIEntry::fetch() does NOT throw for) is a
  // real failure of this lookup, not something to quietly paper over with
  // FreeDictionaryAPI data alone - propagates via fetchEntryOf just like
  // FreeDictionaryAPIEntry's own failures below.
  detail::fetchEntryOf<WiktionaryAPIEntry>(entries, normalizedWord, serverDataDir, languageCode,
                                            ignoreCache);

  // A word genuinely not being found is not an error here at all -
  // FreeDictionaryAPIEntry::fetch() returns nullopt for that
  // (freedictionaryapi.com itself returns HTTP 200 with an empty entries
  // array, not a 404) - so a real failure from inside fetchEntryOf below is
  // always genuine (network, parse, unexpected status), never just "not
  // found".
  detail::fetchEntryOf<FreeDictionaryAPIEntry>(entries, normalizedWord, serverDataDir, languageCode,
                                                ignoreCache);

  // No "a source failed outright" case to check for here anymore - a real
  // failure from either source already threw above, unconditionally, so
  // reaching this point means both calls succeeded (found the word or
  // not - either way, not an error). The only remaining question is
  // whether either source actually found anything.
  if (entries.empty()) {
    return std::nullopt;
  }

  // entries is ordered Wiktionary-then-FreeDictionaryAPIEntry (only
  // whichever ones were actually found): every field uses the same generic
  // rule - whichever source is listed first with non-empty data for that
  // field wins. Wiktionary listed first so phonetics prefers its real
  // audio; meanings/sourceUrl/license are unaffected by list order since
  // only FreeDictionaryAPIEntry ever contributes them; word prefers
  // Wiktionary's echo over FreeDictionaryAPIEntry's when both are
  // non-empty, same as every other field.
  DictionaryEntry entry = detail::mergeDictionaryEntries(entries, normalizedWord);

  try {
    store.write(entryCachePath, detail::dictionaryEntryToJson(entry).dump());
  } catch (const server_data::ServerDataError&) {
    // Best-effort - a cache write failure shouldn't fail an otherwise-good
    // lookup the caller is about to receive anyway.
  }

  return entry;
}

DictionaryEntry dictionaryEntryFrom(const WordInfo& freeDict) {
  DictionaryEntry entry;
  entry.word = freeDict.word;
  entry.meanings = detail::buildMeanings(freeDict.entries);
  entry.sourceUrl = freeDict.source.url;
  entry.license = freeDict.source.license.name;
  entry.phonetics = detail::buildPhoneticsFromFreeDictionary(freeDict.entries);
  return entry;
}

DictionaryEntry dictionaryEntryFrom(const WiktionaryWordInfo& wikt) {
  std::string cachedUsAudioRelativePath;
  std::vector<std::string> cachedUsAudioAccents;
  const AudioPronunciation* primary = wikt.usAudio();
  if (primary && !primary->audio.empty()) {
    cachedUsAudioRelativePath = primary->audio;
    cachedUsAudioAccents = primary->accents;
  }

  DictionaryEntry entry;
  entry.word = wikt.word;
  entry.phonetics = detail::buildPhonetics(wikt.ipa, cachedUsAudioRelativePath, cachedUsAudioAccents);
  return entry;
}

namespace detail {

std::string normalizeAccentLabel(const std::vector<std::string>& rawAccents) {
  bool isUk = false;
  bool isAu = false;

  for (const auto& raw : rawAccents) {
    if (isUsAccentTag(raw)) return "US";  // US always wins - see header comment

    std::string lower = raw;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    if (lower == "uk" || lower == "rp" || lower == "british" ||
        lower.find("received pronunciation") != std::string::npos) {
      isUk = true;
    } else if (lower == "au" || lower.find("australia") != std::string::npos) {
      isAu = true;
    }
  }

  if (isUk) return "UK";
  if (isAu) return "AU";
  return "";
}

std::vector<MeaningEntry> buildMeanings(const std::vector<Entry>& entries) {
  std::vector<MeaningEntry> meanings;
  for (const auto& entry : entries) {
    MeaningEntry meaning;
    meaning.partOfSpeech = entry.partOfSpeech;
    for (const auto& sense : entry.senses) {
      DefinitionEntry def;
      def.definition = sense.definition;
      def.examples = sense.examples;
      def.synonyms = sense.synonyms;
      def.antonyms = sense.antonyms;
      meaning.definitions.push_back(std::move(def));
    }
    meanings.push_back(std::move(meaning));
  }
  return meanings;
}

namespace {

std::string toLowerCopy(const std::string& s) {
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  return lower;
}

// Whether every tag in `tags` also appears (case-insensitively) in
// `supersetTags` - recognizes a non-geographic qualifier tag (e.g. "strong
// form", "cot-caught") on an IPA line as the same pronunciation as a US
// audio entry carrying that same qualifier alongside its country tag.
// `tags` empty returns false deliberately - an untagged line is handled
// separately by the untagged-merge rule below, not by this one (an empty
// set would otherwise be trivially "a subset" of anything).
bool isAccentSubset(const std::vector<std::string>& tags, const std::vector<std::string>& supersetTags) {
  if (tags.empty()) return false;
  for (const auto& tag : tags) {
    std::string lowerTag = toLowerCopy(tag);
    bool found = false;
    for (const auto& superTag : supersetTags) {
      if (toLowerCopy(superTag) == lowerTag) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

// Whether `tag` marks the letter-name reading of a single-letter entry
// (e.g. "ell" for "L"), as opposed to its raw phoneme value (e.g. "l") -
// Wiktionary's own {{q|name of letter}}/{{sense|letter name}} qualifiers,
// folded into this IPA line's accents like any other qualifier (see
// parsePronunciationWikitext's "q"/"sense" handling).
bool isLetterNameTag(const std::string& tag) {
  std::string lower = toLowerCopy(tag);
  return lower == "letter name" || lower == "name of letter";
}

}  // namespace

std::vector<PhoneticEntry> buildPhonetics(const std::vector<IpaPronunciation>& ipa,
                                           const std::string& cachedUsAudioRelativePath,
                                           const std::vector<std::string>& cachedUsAudioAccents) {
  std::vector<PhoneticEntry> rows;
  bool anyUsRow = false;

  // Found up front, before any other row gets a chance to claim the cached
  // audio - Wiktionary's own convention: an IPA line with no accent tag at
  // all applies regardless of dialect (the page didn't bother splitting it
  // out), real example "lesson" (one untagged line, both a UK and a US
  // recording nested directly beneath it). This has to run *before* the
  // per-row literal/qualifier checks below, not just as a fallback after -
  // real bug on "afraid": its untagged line is the one the US audio is
  // actually nested under in the source, but a *different*, explicitly
  // "Southern US"-tagged line was processed first and grabbed the audio for
  // itself (isUsAccentTag recognizes "Southern US" as literal US), leaving
  // the real untagged/US-matched line with neither text-with-audio nor a
  // label - technically "complete" (something got labeled US) but wrong.
  int untaggedIndex = -1;
  int untaggedCount = 0;
  for (size_t i = 0; i < ipa.size(); ++i) {
    if (ipa[i].text.empty()) continue;
    // Tracked via the raw accent list, not the normalized label - a line
    // tagged with something we don't map to a region (e.g. "nonstandard")
    // still has a non-empty `accents`, and shouldn't be confused with a
    // line that carries no accent tag at all.
    if (ipa[i].accents.empty()) {
      untaggedIndex = static_cast<int>(i);
      ++untaggedCount;
    }
  }
  // Skipped when there's more than one untagged line - no way to tell which
  // one the audio applies to (that's Category G's own open question).
  bool reserveUntaggedForAudio = !cachedUsAudioRelativePath.empty() && untaggedCount == 1;

  for (size_t i = 0; i < ipa.size(); ++i) {
    const auto& entry = ipa[i];
    if (entry.text.empty()) continue;
    PhoneticEntry row;
    row.text = entry.text;

    if (reserveUntaggedForAudio && static_cast<int>(i) == untaggedIndex && !anyUsRow) {
      row.label = "US";
    } else {
      row.label = normalizeAccentLabel(entry.accents);
      if (row.label != "US" && !cachedUsAudioAccents.empty() &&
          isAccentSubset(entry.accents, cachedUsAudioAccents)) {
        row.label = "US";
      }
      // A narrower fallback than the subset rule above: no shared
      // qualifier to cross-reference, but this line is specifically
      // marked as the letter-name reading (see isLetterNameTag) - real
      // examples "l"/"c"/"e", where the cached audio carries no qualifier
      // of its own (just a plain "US") to compare against in the first
      // place.
      if (row.label != "US" && !cachedUsAudioRelativePath.empty() &&
          std::any_of(entry.accents.begin(), entry.accents.end(), isLetterNameTag)) {
        row.label = "US";
      }
    }

    if (row.label == "US" && !cachedUsAudioRelativePath.empty() && !anyUsRow) {
      row.audio = cachedUsAudioRelativePath;
      anyUsRow = true;
    }
    rows.push_back(std::move(row));
  }

  // The cached audio's accent didn't match any IPA row above - append an
  // audio-only row rather than silently dropping a real recording.
  if (!cachedUsAudioRelativePath.empty() && !anyUsRow) {
    PhoneticEntry audioOnly;
    audioOnly.label = "US";
    audioOnly.audio = cachedUsAudioRelativePath;
    rows.push_back(std::move(audioOnly));
  }

  return rows;
}

std::vector<PhoneticEntry> buildPhoneticsFromFreeDictionary(const std::vector<Entry>& entries) {
  std::vector<PhoneticEntry> rows;
  for (const auto& entry : entries) {
    for (const auto& p : entry.pronunciations) {
      if (p.text.empty()) continue;
      PhoneticEntry row;
      row.text = p.text;
      row.label = normalizeAccentLabel(p.tags);
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

DictionaryEntry dictionaryEntryFrom(const WordInfo& freeDict) {
  DictionaryEntry entry;
  entry.word = freeDict.word;
  entry.meanings = buildMeanings(freeDict.entries);
  entry.sourceUrl = freeDict.source.url;
  entry.license = freeDict.source.license.name;
  entry.phonetics = buildPhoneticsFromFreeDictionary(freeDict.entries);
  return entry;
}

DictionaryEntry dictionaryEntryFrom(const WiktionaryWordInfo& wikt) {
  std::string cachedUsAudioRelativePath;
  std::vector<std::string> cachedUsAudioAccents;
  const AudioPronunciation* primary = wikt.usAudio();
  if (primary && !primary->audio.empty()) {
    cachedUsAudioRelativePath = primary->audio;
    cachedUsAudioAccents = primary->accents;
  }

  DictionaryEntry entry;
  entry.word = wikt.word;
  entry.phonetics = buildPhonetics(wikt.ipa, cachedUsAudioRelativePath, cachedUsAudioAccents);
  return entry;
}

namespace {

// A phonetics array's own richness, purely from its content - never from
// which source produced it or where it sits in the merge order (see
// mergeDictionaryEntries below for why that distinction matters).
enum class PhoneticsRichness {
  Neither = 0,
  IpaOnly = 1,
  AudioOnly = 2,
  Both = 3,
};

PhoneticsRichness phoneticsRichnessOf(const std::vector<PhoneticEntry>& phonetics) {
  bool hasAudio = false;
  bool hasIpa = false;
  for (const auto& row : phonetics) {
    if (!row.audio.empty()) hasAudio = true;
    if (!row.text.empty()) hasIpa = true;
  }
  if (hasAudio && hasIpa) return PhoneticsRichness::Both;
  if (hasAudio) return PhoneticsRichness::AudioOnly;
  if (hasIpa) return PhoneticsRichness::IpaOnly;
  return PhoneticsRichness::Neither;
}

}  // namespace

DictionaryEntry mergeDictionaryEntries(const std::vector<DictionaryEntry>& entries,
                                        const std::string& fallbackWord) {
  DictionaryEntry merged;
  PhoneticsRichness bestPhoneticsRichness = PhoneticsRichness::Neither;

  for (const auto& entry : entries) {
    if (merged.word.empty() && !entry.word.empty()) merged.word = entry.word;

    // Content-aware, unlike every field below - see this function's own
    // doc comment (dictionary_entry_detail.h) for why phonetics can't use
    // the same plain first-non-empty-wins rule without silently risking a
    // real recording getting discarded in favor of a plain-IPA
    // contribution, purely because of list order.
    PhoneticsRichness richness = phoneticsRichnessOf(entry.phonetics);
    if (richness > bestPhoneticsRichness) {
      merged.phonetics = entry.phonetics;
      bestPhoneticsRichness = richness;
    }

    if (merged.meanings.empty() && !entry.meanings.empty()) merged.meanings = entry.meanings;
    if (merged.sourceUrl.empty() && !entry.sourceUrl.empty()) merged.sourceUrl = entry.sourceUrl;
    if (merged.license.empty() && !entry.license.empty()) merged.license = entry.license;
  }
  if (merged.word.empty()) merged.word = fallbackWord;
  return merged;
}

namespace {

std::string percentEncodeForFilename(const std::string& value) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[(c >> 4) & 0xF]);
      out.push_back(hex[c & 0xF]);
    }
  }
  return out;
}

}  // namespace

std::string entryCacheRelativePath(const std::string& languageCode, const std::string& word) {
  return "dictionaries/entries/" + percentEncodeForFilename(languageCode) + "-" +
         percentEncodeForFilename(word) + ".json";
}

nlohmann::json dictionaryEntryToJson(const DictionaryEntry& entry) {
  nlohmann::json phonetics = nlohmann::json::array();
  for (const auto& p : entry.phonetics) {
    phonetics.push_back({{"text", p.text}, {"label", p.label}, {"audio", p.audio}});
  }

  nlohmann::json meanings = nlohmann::json::array();
  for (const auto& meaning : entry.meanings) {
    nlohmann::json defs = nlohmann::json::array();
    for (const auto& def : meaning.definitions) {
      defs.push_back({{"definition", def.definition},
                       {"examples", def.examples},
                       {"synonyms", def.synonyms},
                       {"antonyms", def.antonyms}});
    }
    meanings.push_back({{"partOfSpeech", meaning.partOfSpeech}, {"definitions", defs}});
  }

  return {{"word", entry.word},
          {"phonetics", phonetics},
          {"meanings", meanings},
          {"sourceUrl", entry.sourceUrl},
          {"license", entry.license}};
}

DictionaryEntry dictionaryEntryFromJson(const nlohmann::json& data) {
  DictionaryEntry entry;
  entry.word = data.value("word", "");

  if (data.contains("phonetics") && data["phonetics"].is_array()) {
    for (const auto& p : data["phonetics"]) {
      PhoneticEntry row;
      row.text = p.value("text", "");
      row.label = p.value("label", "");
      row.audio = p.value("audio", "");
      entry.phonetics.push_back(std::move(row));
    }
  }

  if (data.contains("meanings") && data["meanings"].is_array()) {
    for (const auto& m : data["meanings"]) {
      MeaningEntry meaning;
      meaning.partOfSpeech = m.value("partOfSpeech", "");
      if (m.contains("definitions") && m["definitions"].is_array()) {
        for (const auto& d : m["definitions"]) {
          DefinitionEntry def;
          def.definition = d.value("definition", "");
          def.examples = d.value("examples", std::vector<std::string>{});
          def.synonyms = d.value("synonyms", std::vector<std::string>{});
          def.antonyms = d.value("antonyms", std::vector<std::string>{});
          meaning.definitions.push_back(std::move(def));
        }
      }
      entry.meanings.push_back(std::move(meaning));
    }
  }

  entry.sourceUrl = data.value("sourceUrl", "");
  entry.license = data.value("license", "");

  return entry;
}

}  // namespace detail

}  // namespace dictionary_utils
