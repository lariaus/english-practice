#include "dictionary_utils/wiktionary_api_entry.h"

#include "wiktionary_parsing_detail.h"

#include <https_client/http_client.h>
#include <nlohmann/json.hpp>
#include <server_data/server_data_store.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace dictionary_utils {

namespace {

constexpr const char* kApiBase = "https://en.wiktionary.org/w/api.php";

// Wikimedia's API etiquette requires a contactable User-Agent identifying
// the client - see https://meta.wikimedia.org/wiki/User-Agent_policy.
// Unlike a browser's fetch(), NSURLRequest doesn't block overriding this.
constexpr const char* kUserAgent =
    "EnglishPracticeApp/1.0 (personal language-learning app; native-server dictionary_utils)";

std::string percentEncodeQueryValue(const std::string& value) {
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

nlohmann::json getJson(https_client::HttpClient& client, const std::string& url) {
  https_client::HttpResponse res;
  try {
    res = client.get(url, {{"Accept", "application/json"}, {"User-Agent", kUserAgent}});
  } catch (const https_client::HttpError& e) {
    throw WiktionaryApiError(std::string("Could not reach Wiktionary: ") + e.what());
  }
  if (res.statusCode != 200) {
    throw WiktionaryApiError("Unexpected status from Wiktionary: " + std::to_string(res.statusCode));
  }
  try {
    return nlohmann::json::parse(res.body);
  } catch (const nlohmann::json::parse_error& e) {
    throw WiktionaryApiError(std::string("Could not parse Wiktionary response: ") + e.what());
  }
}

std::string cacheRelativePath(const std::string& languageCode, const std::string& word) {
  return "dictionaries/wiktionaryapi/" + percentEncodeQueryValue(languageCode) + "-" +
         percentEncodeQueryValue(word) + ".json";
}

// Resolves and downloads the primary audio candidate in place. A candidate
// simply not existing (no US-tagged audio on the page at all) is handled
// by the caller before this is ever invoked - that's a normal outcome, not
// a failure. Once we're here, though, a real attempt is being made: any
// failure resolving or fetching that specific file - imageinfo erroring,
// the response not resolving to a URL, the actual GET failing or
// returning non-200, or the disk write failing - is treated as a genuine
// failure of the whole lookup and thrown as WiktionaryApiError, the same
// type WiktionaryAPIEntry::fetch() already throws elsewhere and that
// DictionaryEntry::fetch() already catches as "this source failed" - audio
// is the entire reason this class exists, so a candidate we found but
// couldn't actually fetch is not something to silently paper over.
//
// The final audio-bytes GET below (not the imageinfo query just above it,
// which - like every other request in this file - hits en.wiktionary.org)
// sends a Referer of the Wiktionary page the audio was found on.
// upload.wikimedia.org's edge layer rate-limits non-browser traffic
// aggressively, partly based on a missing Referer header (see
// phabricator.wikimedia.org/T413570) - a real browser would send exactly
// this value, since the audio is embedded directly on that page rather
// than navigated to via Commons' own file-description page. No other
// request in this file ever touches upload.wikimedia.org.
void downloadPrimaryAudio(https_client::HttpClient& client, server_data::ServerDataStore& store,
                           AudioPronunciation& a, bool ignoreCache, const std::string& word) {
  // Lives alongside this class's own per-word JSON cache
  // (dictionaries/wiktionaryapi/<lang>-<word>.json) rather than a shared
  // flat location - it's Wiktionary's own data, fetched only by this class.
  std::string relativePath = "dictionaries/wiktionaryapi/" + a.filename;

  bool alreadyCached = store.exists(relativePath);
  if (!detail::shouldDownloadAudio(alreadyCached, ignoreCache)) {
    a.audio = relativePath;
    return;
  }

  nlohmann::json imageinfoResponse;
  try {
    // "|" left unencoded here used to trip newer NSURL's lenient auto-
    // percent-encoding (see http_client.mm's NSURL URLWithString: call) -
    // when NSURL "fixes up" that one invalid raw character, it also
    // re-escapes the %XX sequences percentEncodeQueryValue() already
    // produced for `a.filename`, corrupting any filename with a space or
    // paren (e.g. "incredible"'s Commons files) into a literal double-
    // encoded title Wiktionary's API then rejects. %7C avoids triggering
    // that fixup at all.
    std::string imageinfoUrl = std::string(kApiBase) + "?action=query&titles=File:" +
                                percentEncodeQueryValue(a.filename) +
                                "&prop=imageinfo&iiprop=url%7Cmime%7Csize&format=json";
    imageinfoResponse = getJson(client, imageinfoUrl);
  } catch (const WiktionaryApiError& e) {
    std::cerr << "[WiktionaryAPIEntry] audio download failed for \"" << a.filename
              << "\": imageinfo query errored: " << e.what() << "\n";
    throw;
  }

  auto resolved = detail::resolveAudioMetadata(imageinfoResponse);
  if (!resolved || resolved->url.empty()) {
    std::string message = "audio download failed for \"" + a.filename +
                           "\": imageinfo response did not resolve to a usable URL - raw "
                           "response: " +
                           imageinfoResponse.dump();
    std::cerr << "[WiktionaryAPIEntry] " << message << "\n";
    throw WiktionaryApiError(message);
  }

  std::string referer = "https://en.wiktionary.org/wiki/" + percentEncodeQueryValue(word);

  https_client::HttpResponse audioRes;
  try {
    audioRes = client.get(resolved->url, {{"Referer", referer}});
  } catch (const https_client::HttpError& e) {
    std::cerr << "[WiktionaryAPIEntry] audio download failed for \"" << a.filename
              << "\": GET " << resolved->url << " errored: " << e.what() << "\n";
    throw WiktionaryApiError(std::string("audio download failed: ") + e.what());
  }
  if (audioRes.statusCode != 200) {
    std::string message = "audio download failed for \"" + a.filename + "\": GET " +
                           resolved->url + " returned status " +
                           std::to_string(audioRes.statusCode);
    std::cerr << "[WiktionaryAPIEntry] " << message << "\n";
    throw WiktionaryApiError(message);
  }

  try {
    store.write(relativePath, audioRes.body);
  } catch (const server_data::ServerDataError& e) {
    std::cerr << "[WiktionaryAPIEntry] audio download failed for \"" << a.filename
              << "\": writing to \"" << relativePath << "\" errored: " << e.what() << "\n";
    throw WiktionaryApiError(std::string("audio download failed: ") + e.what());
  }

  a.audio = relativePath;
  a.mimeType = resolved->mimeType;
  a.durationSeconds = resolved->durationSeconds;
  a.sizeBytes = resolved->sizeBytes;
}

// US state names, lowercased. Some Wiktionary audio/IPA accent tags name a
// specific US state instead of "US"/"GA"/"GenAm" (e.g. "Colorado", "Texas",
// "New Mexico") - real examples on "vulnerability", "respect", and
// "similarity", none of which have an "en-us-..." filename to fall back on
// either (they're Lingua Libre recordings, named
// "LL-Q1860 (eng)-<contributor>-<word>" regardless of accent), so the tag
// itself is the only signal available. A curated, finite list - not
// exhaustive by nature, but the 50 states cover the overwhelming majority
// of real cases; "Georgia" is a known ambiguity with the country, accepted
// since this only ever runs against English-language accent tags, where a
// bare "Georgia" tag is always the US state.
const std::unordered_set<std::string>& usStateNames() {
  static const std::unordered_set<std::string> states = {
      "alabama",       "alaska",         "arizona",        "arkansas",
      "california",    "colorado",       "connecticut",    "delaware",
      "florida",       "georgia",        "hawaii",         "idaho",
      "illinois",       "indiana",        "iowa",           "kansas",
      "kentucky",       "louisiana",      "maine",          "maryland",
      "massachusetts",  "michigan",       "minnesota",      "mississippi",
      "missouri",       "montana",        "nebraska",       "nevada",
      "new hampshire",  "new jersey",     "new mexico",     "new york",
      "north carolina", "north dakota",   "ohio",           "oklahoma",
      "oregon",         "pennsylvania",   "rhode island",   "south carolina",
      "south dakota",   "tennessee",      "texas",          "utah",
      "vermont",        "virginia",       "washington",     "west virginia",
      "wisconsin",      "wyoming",
  };
  return states;
}

// Whether `lowerTag` names a US state, optionally with a leading compass
// qualifier - e.g. "Western Pennsylvania" (real tag seen on "respect").
bool isUsStateOrRegionTag(const std::string& lowerTag) {
  if (usStateNames().count(lowerTag)) return true;

  static const std::vector<std::string> kCompassPrefixes = {
      "northern ", "southern ", "eastern ", "western ", "upper ", "lower ", "central ",
  };
  for (const auto& prefix : kCompassPrefixes) {
    if (lowerTag.rfind(prefix, 0) == 0 && usStateNames().count(lowerTag.substr(prefix.size()))) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool isUsAccentTag(const std::string& tag) {
  std::string lower = tag;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  if (lower == "us" || lower == "ga" || lower == "genam" ||
      lower.find("general american") != std::string::npos ||
      lower.find("north american") != std::string::npos) {
    return true;
  }

  // Compound US dialect labels like "Midland US", "Southern US", "Northern
  // US" - a real Wiktionary convention (seen on "online") for tagging a
  // specific American sub-dialect rather than just "US"/"GenAm"/"GA". Any
  // whole word "us" in the tag counts, not just the tag being exactly "us".
  std::istringstream words(lower);
  std::string word;
  while (words >> word) {
    if (word == "us") return true;
  }

  if (isUsStateOrRegionTag(lower)) return true;

  return false;
}

bool hasUsAccent(const std::vector<std::string>& accents) {
  return std::any_of(accents.begin(), accents.end(), isUsAccentTag);
}

bool isUsAudioCandidate(const AudioPronunciation& audio) {
  if (hasUsAccent(audio.accents)) return true;

  std::string lowerFilename = audio.filename;
  std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  return lowerFilename.rfind("en-us-", 0) == 0;
}

const AudioPronunciation* WiktionaryWordInfo::usAudio() const {
  // Prefer whichever US candidate was actually downloaded (a non-empty
  // `audio` path) - fetch() may have picked a different one than "the
  // first US-tagged candidate" (see detail::selectPrimaryUsAudio), and
  // that's the one whose bytes actually exist. Falls back to the plain
  // first-US-tagged rule when nothing has been downloaded yet (e.g. a
  // WiktionaryWordInfo built directly in a test, without ever going
  // through fetch()).
  for (const auto& a : audio) {
    if (isUsAudioCandidate(a) && !a.audio.empty()) return &a;
  }
  for (const auto& a : audio) {
    if (isUsAudioCandidate(a)) return &a;
  }
  return nullptr;
}

const IpaPronunciation* WiktionaryWordInfo::usIpa() const {
  for (const auto& p : ipa) {
    if (hasUsAccent(p.accents)) return &p;
  }
  return nullptr;
}

std::optional<WiktionaryWordInfo> WiktionaryAPIEntry::fetch(const std::string& word,
                                                             const std::filesystem::path& serverDataDir,
                                                             const std::string& languageCode,
                                                             bool ignoreCache) {
  server_data::ServerDataStore store(serverDataDir);
  std::string cachePath = cacheRelativePath(languageCode, word);

  if (!ignoreCache) {
    auto cached = store.read(cachePath);
    if (cached) {
      try {
        // A not-found result is never written to this cache (see below), so
        // a cache file existing always means the word was found - nothing
        // further to check here.
        return detail::wiktionaryWordInfoFromJson(nlohmann::json::parse(*cached));
      } catch (const nlohmann::json::exception&) {
        // Corrupt/unreadable cache file - fall through to a live lookup
        // rather than failing a lookup that would otherwise succeed.
      }
    }
  }

  WiktionaryWordInfo info;
  info.word = word;
  // Only used to decide this call's own return value and whether to write
  // the cache file below, a detail of this function alone.
  bool found = false;

  https_client::HttpClient client;

  std::string tocUrl = std::string(kApiBase) + "?action=parse&page=" +
                        percentEncodeQueryValue(word) + "&prop=tocdata&format=json";
  nlohmann::json tocResponse = getJson(client, tocUrl);

  bool pageMissing = false;
  if (tocResponse.contains("error")) {
    std::string code = tocResponse["error"].value("code", "");
    if (code == "missingtitle") {
      pageMissing = true;  // page doesn't exist - expected, not an error
    } else {
      throw WiktionaryApiError("Wiktionary API error looking up \"" + word +
                                "\": " + tocResponse["error"].value("info", code));
    }
  }

  if (!pageMissing) {
    auto sections = detail::parseTocSections(tocResponse);
    auto lookup =
        detail::findPronunciationSection(sections, detail::languageSectionForCode(languageCode));
    found = lookup.languageFound;

    if (lookup.pronunciationFound) {
      info.hasPronunciationSection = true;

      // Usually exactly one candidate. When a word splits pronunciation by
      // sense (e.g. "interrupt": verb vs noun, each its own numbered
      // "Pronunciation N" heading), the first one found has precedence -
      // only fetch and try the next candidate if the current one doesn't
      // satisfy detail::hasIpaAndUsAudio().
      std::vector<detail::ParsedPronunciationWikitext> candidates;
      for (const auto& sectionIndex : lookup.sectionIndices) {
        std::string wikitextUrl = std::string(kApiBase) + "?action=parse&page=" +
                                   percentEncodeQueryValue(word) +
                                   "&section=" + sectionIndex + "&prop=wikitext&format=json";
        nlohmann::json wikitextResponse = getJson(client, wikitextUrl);
        std::string wikitext = wikitextResponse["parse"]["wikitext"].value("*", "");

        candidates.push_back(detail::parsePronunciationWikitext(wikitext));
        if (detail::hasIpaAndUsAudio(candidates.back())) break;
      }

      auto selected = detail::selectPronunciationSection(candidates);
      info.audio = std::move(selected.audio);
      info.ipa = std::move(selected.ipa);

      std::cerr << "[WiktionaryAPIEntry] \"" << word << "\": parsed " << info.audio.size()
                << " audio candidate(s):";
      for (const auto& a : info.audio) {
        std::cerr << " {file=\"" << a.filename << "\", accents=[";
        for (size_t i = 0; i < a.accents.size(); ++i) {
          if (i) std::cerr << ",";
          std::cerr << a.accents[i];
        }
        std::cerr << "], us=" << (isUsAudioCandidate(a) ? "yes" : "no") << "}";
      }
      std::cerr << "\n";

      if (AudioPronunciation* primary = detail::selectPrimaryUsAudio(info.audio, info.ipa)) {
        downloadPrimaryAudio(client, store, *primary, ignoreCache, word);
      }
    }
  }

  if (!found) return std::nullopt;

  try {
    store.write(cachePath, detail::wiktionaryWordInfoToJson(info).dump());
  } catch (const server_data::ServerDataError&) {
    // Best-effort - a cache write failure shouldn't fail an otherwise-good
    // lookup the caller is about to receive anyway.
  }

  return info;
}

bool WiktionaryAPIEntry::hasCachedEntry(const std::string& word,
                                         const std::filesystem::path& serverDataDir,
                                         const std::string& languageCode) {
  server_data::ServerDataStore store(serverDataDir);
  return store.exists(cacheRelativePath(languageCode, word));
}

std::optional<WiktionaryWordInfo> WiktionaryAPIEntry::fetchFromCache(
    const std::string& word, const std::filesystem::path& serverDataDir,
    const std::string& languageCode) {
  server_data::ServerDataStore store(serverDataDir);
  auto cached = store.read(cacheRelativePath(languageCode, word));
  if (!cached) return std::nullopt;

  try {
    return detail::wiktionaryWordInfoFromJson(nlohmann::json::parse(*cached));
  } catch (const nlohmann::json::exception&) {
    return std::nullopt;
  }
}

namespace detail {

namespace {

std::string trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\n\r");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\n\r");
  return s.substr(b, e - b + 1);
}

std::vector<std::string> splitOnChar(const std::string& s, char delimiter) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == delimiter) {
      parts.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  return parts;
}

// Like splitOnChar, but treats anything inside a nested {{...}} template as
// opaque - a delimiter inside a nested template never counts as a
// separator. Needed for splitting a template's own pipe-separated parameter
// list specifically: a parameter's value can itself be a nested template
// with its own pipes (real example, "mommy"'s
// {{audio|en|En-us-mommy.ogg|IPA=/ˈmɑ.mi/|3={{a|en|GA}}}}) - naively
// splitting on every "|" would shred that nested {{a|en|GA}} across two
// bogus extra "parameters" instead of keeping its value intact.
std::vector<std::string> splitTopLevelParams(const std::string& s, char delimiter) {
  std::vector<std::string> parts;
  size_t start = 0;
  int depth = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '{' && i + 1 < s.size() && s[i + 1] == '{') {
      ++depth;
      ++i;
    } else if (s[i] == '}' && i + 1 < s.size() && s[i + 1] == '}') {
      if (depth > 0) --depth;
      ++i;
    } else if (s[i] == delimiter && depth == 0) {
      parts.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  parts.push_back(s.substr(start));
  return parts;
}

// Whether `line` is a Pronunciation-like section heading: a plain
// "Pronunciation", or a numbered "Pronunciation 1", "Pronunciation 2", etc.
// - Wiktionary's convention when a word has multiple senses with different
// pronunciations (e.g. "interrupt": the verb and the noun stress
// differently, so the page splits them into separate numbered sections
// instead of one shared "Pronunciation").
bool isPronunciationHeading(const std::string& line) {
  if (line == "Pronunciation") return true;

  static const std::string kNumberedPrefix = "Pronunciation ";
  if (line.rfind(kNumberedPrefix, 0) != 0) return false;
  std::string suffix = line.substr(kNumberedPrefix.size());
  return !suffix.empty() &&
         std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) { return std::isdigit(c); });
}

std::vector<std::string> parseAccentParam(const std::string& value) {
  std::vector<std::string> out;
  for (const auto& tag : splitOnChar(value, ',')) {
    std::string t = trim(tag);
    if (!t.empty()) out.push_back(t);
  }
  return out;
}

// Whether `part` looks like a MediaWiki named parameter ("key=value") rather
// than a plain positional value - real IPA text (always starts with "/" or
// "[") and audio filenames never match this shape, so it's a safe way to
// tell the two apart.
bool looksLikeNamedParam(const std::string& part) {
  size_t eq = part.find('=');
  if (eq == std::string::npos || eq == 0) return false;
  for (size_t i = 0; i < eq; ++i) {
    if (!std::isalnum(static_cast<unsigned char>(part[i]))) return false;
  }
  return true;
}

// A template's positional (unnamed) parameters, in order, skipping any
// "key=value" ones - real MediaWiki semantics: a named parameter never
// consumes a positional slot. Needed because some pages put "a="/"aa=" (or
// other named params) *before* the actual text/filename positional param,
// e.g. "technology"'s
//   {{IPA|en|a=India|/ʈek(h).noˈlɔː.dʒiː/|/ʈek(h)ˈnɔː.lə.dʒiː/}}
// A naive "parts[2] is always the text" assumption would capture the
// literal string "a=India" as the transcription instead of the real IPA.
std::vector<std::string> positionalParams(const std::vector<std::string>& parts) {
  std::vector<std::string> out;
  for (size_t i = 1; i < parts.size(); ++i) {
    std::string p = trim(parts[i]);
    if (!looksLikeNamedParam(p)) out.push_back(p);
  }
  return out;
}

// Wikitext list nesting depth - a line's leading run of '*'/':' characters,
// e.g. "** {{IPA|...}}" is depth 2. Both count: '*' is the usual bulleted-
// list marker, but some pages (e.g. "police", "c", "e") nest with ':'
// (MediaWiki's definition-list indent) instead - real example, "police":
//   * {{a|en|GA|CA}}
//   :: {{IPA|en|/pəˈlis/}}
// Without counting ':' here, that {{a|...}} block's own scope (tracked via
// this depth) would immediately pop before ever reaching its "::"-indented
// children, since a plain '*'-only depth would read them as depth 0 - at or
// shallower than the block's own depth 1. MediaWiki list markup must start
// at column zero, so no leading whitespace to skip first.
int bulletDepth(const std::string& line) {
  size_t i = 0;
  while (i < line.size() && (line[i] == '*' || line[i] == ':')) ++i;
  return static_cast<int>(i);
}

// Finds every top-level {{...}} template on `line`, returning each one's
// inner content (everything between the outer "{{" and its matching "}}").
// A brace-depth scan rather than a non-nested regex, so a template with
// another template embedded inside one of its own parameters - e.g. a
// citation reference, real example from "dictator":
//   {{IPA|en|.../a=GA|a3=dated|ref3={{R:en:Pyles:1972|432}}}}
// - is still matched as a single whole template (with the nested
// {{R:...}} left intact inside the returned string), instead of the regex
// approach's failure mode: unable to match any "{{...}}" span containing
// nested braces at all, so the entire outer template - including its
// perfectly good "a=GA" - was silently invisible to every check below.
std::vector<std::string> findTopLevelTemplates(const std::string& line) {
  std::vector<std::string> templates;
  size_t i = 0;
  while (i < line.size()) {
    if (line[i] == '{' && i + 1 < line.size() && line[i + 1] == '{') {
      size_t start = i + 2;
      int depth = 1;
      size_t j = start;
      while (j < line.size() && depth > 0) {
        if (line[j] == '{' && j + 1 < line.size() && line[j + 1] == '{') {
          ++depth;
          j += 2;
        } else if (line[j] == '}' && j + 1 < line.size() && line[j + 1] == '}') {
          --depth;
          j += 2;
        } else {
          ++j;
        }
      }
      if (depth == 0) {
        templates.push_back(line.substr(start, (j - 2) - start));
        i = j;
      } else {
        break;  // unterminated "{{" - nothing more to find on this line
      }
    } else {
      ++i;
    }
  }
  return templates;
}

// Wiktionary sometimes embeds a word's own IPA transcription directly
// inside its {{audio|...}} template via a named "IPA=" parameter, with the
// accent (if any) given by a nested {{a|en|...}} template as another
// parameter's own value, rather than as a separate {{IPA|...}} line - real
// example, "mommy":
//   {{audio|en|En-us-mommy.ogg|IPA=/ˈmɑ.mi/|3={{a|en|GA}}}}
// Returns nullopt if this template has no such embedded "IPA=" parameter at
// all (the overwhelming majority of {{audio|...}} templates).
std::optional<IpaPronunciation> embeddedAudioIpa(const std::vector<std::string>& parts) {
  std::optional<std::string> text;
  std::vector<std::string> accents;

  for (size_t i = 1; i < parts.size(); ++i) {
    std::string p = trim(parts[i]);
    if (p.rfind("IPA=", 0) == 0) {
      text = p.substr(4);
      continue;
    }
    size_t eq = p.find('=');
    if (eq == std::string::npos) continue;
    for (const auto& nested : findTopLevelTemplates(p.substr(eq + 1))) {
      std::vector<std::string> nestedParts = splitTopLevelParams(nested, '|');
      if (nestedParts.empty() || trim(nestedParts[0]) != "a") continue;
      for (size_t j = 2; j < nestedParts.size(); ++j) {
        std::string tag = trim(nestedParts[j]);
        if (!tag.empty()) accents.push_back(tag);
      }
    }
  }

  if (!text) return std::nullopt;
  IpaPronunciation result;
  result.text = trim(*text);
  result.accents = std::move(accents);
  return result;
}

std::vector<std::string> jsonToStringVector(const nlohmann::json& arr) {
  std::vector<std::string> out;
  if (!arr.is_array()) return out;
  for (const auto& item : arr) {
    if (item.is_string()) out.push_back(item.get<std::string>());
  }
  return out;
}

}  // namespace

std::string languageSectionForCode(const std::string& languageCode) {
  if (languageCode == "en") return "English";
  return languageCode;
}

std::vector<TocSection> parseTocSections(const nlohmann::json& tocdataResponse) {
  std::vector<TocSection> out;
  if (!tocdataResponse.contains("parse")) return out;
  const auto& parse = tocdataResponse["parse"];
  if (!parse.contains("tocdata")) return out;
  const auto& sections = parse["tocdata"].value("sections", nlohmann::json::array());
  for (const auto& s : sections) {
    TocSection section;
    section.tocLevel = s.value("tocLevel", 0);
    section.line = s.value("line", "");
    section.index = s.value("index", "");
    out.push_back(std::move(section));
  }
  return out;
}

PronunciationSectionLookup findPronunciationSection(const std::vector<TocSection>& sections,
                                                     const std::string& languageSection) {
  PronunciationSectionLookup result;

  size_t languageIdx = sections.size();
  for (size_t i = 0; i < sections.size(); ++i) {
    if (sections[i].tocLevel == 1 && sections[i].line == languageSection) {
      languageIdx = i;
      break;
    }
  }
  if (languageIdx == sections.size()) return result;
  result.languageFound = true;

  for (size_t j = languageIdx + 1; j < sections.size(); ++j) {
    if (sections[j].tocLevel == 1) break;  // next top-level language section - stop
    if (isPronunciationHeading(sections[j].line)) {
      result.sectionIndices.push_back(sections[j].index);
    }
  }

  if (!result.sectionIndices.empty()) {
    result.pronunciationFound = true;
    result.sectionIndex = result.sectionIndices.front();
  }

  return result;
}

ParsedPronunciationWikitext parsePronunciationWikitext(const std::string& wikitext) {
  ParsedPronunciationWikitext result;

  // Tracks currently "open" {{a|en|...}} block-accent templates - a common
  // Wiktionary convention for tagging a whole group of lines with one
  // shared accent instead of repeating it on each one, e.g. "already":
  //   * {{a|en|GA}}
  //   ** {{IPA|en|/ɑlˈɹɛdi/|a=cot-caught}}
  // Unlike every other template here, {{a|...}}'s own accent tags are
  // positional (parts[2:]), not "a=..." key/value pairs. A block applies to
  // every line nested more deeply than it (by wikitext bullet depth: *, **,
  // ***, ...) until a line at the same or shallower depth appears - stored
  // as a stack (depth, tags) so two nested {{a|...}} lines in a row (each
  // deeper than the last, e.g. "advantage"'s {{a|en|GA|...}} wrapping
  // {{a|en|non-æ-tensing}}) both stay active for their own descendants.
  std::vector<std::pair<int, std::vector<std::string>>> blockStack;

  // Processed one line at a time (rather than scanning the whole wikitext
  // in one pass) because Wiktionary sometimes puts the accent qualifier on
  // a leading {{enPR|...|a=X}} template rather than on the {{IPA|...}}/
  // {{audio|...}} template it's paired with on that same line, e.g. "until":
  //   * {{enPR|...|a=US}} {{IPA|en|/ʌnˈtɪl/|/ənˈtɪl/}}
  // An IPA/audio template with no a= param of its own inherits whatever
  // accent a same-line enPR template carries; one with its own a= param
  // (e.g. the nested "a=nonstandard" IPA line right below that one) is
  // never overridden.
  for (const auto& line : splitOnChar(wikitext, '\n')) {
    std::vector<std::string> lineAccent;
    int depth = bulletDepth(line);

    while (!blockStack.empty() && blockStack.back().first >= depth) {
      blockStack.pop_back();
    }

    for (const auto& inner : findTopLevelTemplates(line)) {
      std::vector<std::string> parts = splitTopLevelParams(inner, '|');
      if (parts.empty()) continue;
      std::string name = trim(parts[0]);

      if (name == "a") {
        std::vector<std::string> blockTags;
        for (size_t i = 2; i < parts.size(); ++i) {
          std::string tag = trim(parts[i]);
          if (!tag.empty()) blockTags.push_back(tag);
        }
        if (!blockTags.empty()) blockStack.emplace_back(depth, std::move(blockTags));
        continue;
      }

      // Recomputed fresh for every template on this line, rather than once
      // per line - a {{a|...}} block declared earlier on this SAME line
      // (e.g. "them": "{{a|en|stressed}} {{IPA|en|/ˈðɛm/}}") must apply to
      // templates processed after it here, not just to templates on
      // subsequent, more deeply-nested lines. For every other line (no
      // same-line {{a|...}}), blockStack is unchanged across the whole
      // line, so this is exactly equivalent to computing it once.
      std::vector<std::string> inheritedAccents;
      for (const auto& block : blockStack) {
        inheritedAccents.insert(inheritedAccents.end(), block.second.begin(), block.second.end());
      }

      std::vector<std::string> accents;
      for (size_t i = 1; i < parts.size(); ++i) {
        std::string p = trim(parts[i]);
        if (p.rfind("a=", 0) == 0) {
          auto more = parseAccentParam(p.substr(2));
          accents.insert(accents.end(), more.begin(), more.end());
        } else if (p.rfind("aa=", 0) == 0) {
          // A second accent-parameter spelling seen in the wild ("door",
          // "lost") - appears to mean "all accents", set directly on
          // individual {{IPA|...}} templates rather than via the {{a|...}}
          // block convention above. Treated identically to "a=".
          auto more = parseAccentParam(p.substr(3));
          accents.insert(accents.end(), more.begin(), more.end());
        }
      }

      if (name == "enPR" && !accents.empty()) lineAccent = accents;

      // {{q|...}}/{{sense|...}} - a register/sense qualifier, not an
      // accent, but carried the same way: real example, single-letter
      // words like "l"/"c"/"e" label the letter-name reading
      // ({{q|name of letter}}/{{sense|letter name}}) separately from the
      // raw phoneme ({{q|phoneme}}/{{sense|phoneme}}), each on its own
      // line with no "a="/"aa=" of its own. Unlike {{a|...}}, these take
      // no leading language-code positional argument.
      if (name == "q" || name == "sense") {
        std::vector<std::string> qualifierTags;
        for (size_t i = 1; i < parts.size(); ++i) {
          std::string tag = trim(parts[i]);
          if (!tag.empty()) qualifierTags.push_back(tag);
        }
        if (!qualifierTags.empty()) lineAccent = qualifierTags;
      }

      std::vector<std::string> positional = positionalParams(parts);

      if (name == "audio" && positional.size() >= 2) {
        AudioPronunciation a;
        a.filename = positional[1];
        a.accents = inheritedAccents;
        auto ownAccents = accents.empty() ? lineAccent : accents;
        a.accents.insert(a.accents.end(), ownAccents.begin(), ownAccents.end());
        if (!a.filename.empty()) result.audio.push_back(std::move(a));

        if (auto embedded = embeddedAudioIpa(parts)) {
          IpaPronunciation p = std::move(*embedded);
          std::vector<std::string> pAccents = inheritedAccents;
          pAccents.insert(pAccents.end(), p.accents.begin(), p.accents.end());
          p.accents = std::move(pAccents);
          if (!p.text.empty()) result.ipa.push_back(std::move(p));
        }
      } else if (name == "IPA" && positional.size() >= 2) {
        IpaPronunciation p;
        p.text = positional[1];
        p.accents = inheritedAccents;
        auto ownAccents = accents.empty() ? lineAccent : accents;
        p.accents.insert(p.accents.end(), ownAccents.begin(), ownAccents.end());
        if (!p.text.empty()) result.ipa.push_back(std::move(p));
      }
    }
  }

  return result;
}

bool hasIpaAndUsAudio(const ParsedPronunciationWikitext& parsed) {
  return !parsed.ipa.empty() &&
         std::any_of(parsed.audio.begin(), parsed.audio.end(), isUsAudioCandidate);
}

namespace {

std::string toLowerCopy(const std::string& s) {
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  return lower;
}

}  // namespace

AudioPronunciation* selectPrimaryUsAudio(std::vector<AudioPronunciation>& audio,
                                          const std::vector<IpaPronunciation>& ipa) {
  AudioPronunciation* firstUs = nullptr;
  for (auto& a : audio) {
    if (!isUsAudioCandidate(a)) continue;
    if (!firstUs) firstUs = &a;
    for (const auto& audioTag : a.accents) {
      std::string lowerAudioTag = toLowerCopy(audioTag);
      for (const auto& p : ipa) {
        for (const auto& ipaTag : p.accents) {
          if (toLowerCopy(ipaTag) == lowerAudioTag) return &a;
        }
      }
    }
  }
  return firstUs;
}

ParsedPronunciationWikitext selectPronunciationSection(
    const std::vector<ParsedPronunciationWikitext>& candidates) {
  for (const auto& candidate : candidates) {
    if (hasIpaAndUsAudio(candidate)) return candidate;
  }
  return candidates.empty() ? ParsedPronunciationWikitext{} : candidates.front();
}

std::optional<ResolvedAudioMetadata> resolveAudioMetadata(const nlohmann::json& imageinfoResponse) {
  if (!imageinfoResponse.contains("query")) return std::nullopt;
  const auto& pages = imageinfoResponse["query"].value("pages", nlohmann::json::object());

  for (auto it = pages.begin(); it != pages.end(); ++it) {
    const auto& page = it.value();
    if (!page.contains("imageinfo") || page["imageinfo"].empty()) continue;
    const auto& info = page["imageinfo"][0];

    ResolvedAudioMetadata result;
    result.url = info.value("url", "");
    result.mimeType = info.value("mime", "");
    result.durationSeconds = info.value("duration", 0.0);
    result.sizeBytes = info.value("size", 0LL);
    return result;
  }
  return std::nullopt;
}

bool shouldDownloadAudio(bool alreadyCached, bool ignoreCache) {
  return !alreadyCached || ignoreCache;
}

nlohmann::json wiktionaryWordInfoToJson(const WiktionaryWordInfo& info) {
  nlohmann::json audio = nlohmann::json::array();
  for (const auto& a : info.audio) {
    audio.push_back({{"filename", a.filename},
                      {"audio", a.audio},
                      {"mimeType", a.mimeType},
                      {"durationSeconds", a.durationSeconds},
                      {"sizeBytes", a.sizeBytes},
                      {"accents", a.accents}});
  }

  nlohmann::json ipa = nlohmann::json::array();
  for (const auto& p : info.ipa) {
    ipa.push_back({{"text", p.text}, {"accents", p.accents}});
  }

  return {{"word", info.word},
          {"audio", audio},
          {"ipa", ipa},
          {"hasPronunciationSection", info.hasPronunciationSection}};
}

WiktionaryWordInfo wiktionaryWordInfoFromJson(const nlohmann::json& data) {
  WiktionaryWordInfo info;
  info.word = data.value("word", "");
  info.hasPronunciationSection = data.value("hasPronunciationSection", false);

  if (data.contains("audio") && data["audio"].is_array()) {
    for (const auto& a : data["audio"]) {
      AudioPronunciation entry;
      entry.filename = a.value("filename", "");
      entry.audio = a.value("audio", "");
      entry.mimeType = a.value("mimeType", "");
      entry.durationSeconds = a.value("durationSeconds", 0.0);
      entry.sizeBytes = a.value("sizeBytes", 0LL);
      entry.accents = jsonToStringVector(a.value("accents", nlohmann::json::array()));
      info.audio.push_back(std::move(entry));
    }
  }

  if (data.contains("ipa") && data["ipa"].is_array()) {
    for (const auto& p : data["ipa"]) {
      IpaPronunciation entry;
      entry.text = p.value("text", "");
      entry.accents = jsonToStringVector(p.value("accents", nlohmann::json::array()));
      info.ipa.push_back(std::move(entry));
    }
  }

  return info;
}

}  // namespace detail

}  // namespace dictionary_utils
