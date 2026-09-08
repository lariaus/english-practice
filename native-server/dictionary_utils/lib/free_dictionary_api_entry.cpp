#include "dictionary_utils/free_dictionary_api_entry.h"

#include "parsing_detail.h"

#include <https_client/http_client.h>
#include <nlohmann/json.hpp>
#include <server_data/server_data_store.h>

#include <cctype>
#include <utility>

namespace dictionary_utils {

namespace {

constexpr const char* kBaseUrl = "https://freedictionaryapi.com/api/v1/entries/";

std::string percentEncodePathSegment(const std::string& value) {
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

// The live HTTP round-trip, returning the raw parsed response - shared by
// both the cache-hit and cache-miss paths of fetch() below, so a cached
// read and a live fetch always parse through the exact same
// detail::parseWordInfo() afterward.
nlohmann::json fetchRawJson(const std::string& word, const std::string& languageCode) {
  std::string url = std::string(kBaseUrl) + languageCode + "/" + percentEncodePathSegment(word);

  https_client::HttpClient client;
  https_client::HttpResponse res;
  try {
    res = client.get(url, {{"Accept", "application/json"}});
  } catch (const https_client::HttpError& e) {
    throw FreeDictionaryApiError(std::string("Could not fetch word info: ") + e.what());
  }
  if (res.statusCode != 200) {
    throw FreeDictionaryApiError("Unexpected status looking up \"" + word +
                                  "\": " + std::to_string(res.statusCode));
  }

  try {
    return nlohmann::json::parse(res.body);
  } catch (const nlohmann::json::parse_error& e) {
    throw FreeDictionaryApiError(std::string("Could not parse word info response: ") + e.what());
  }
}

std::string cacheRelativePath(const std::string& languageCode, const std::string& word) {
  return "dictionaries/freedictionaryapi/" + percentEncodePathSegment(languageCode) + "-" +
         percentEncodePathSegment(word) + ".json";
}

}  // namespace

std::optional<WordInfo> FreeDictionaryAPIEntry::fetch(const std::string& word,
                                                       const std::filesystem::path& serverDataDir,
                                                       const std::string& languageCode,
                                                       bool ignoreCache) {
  server_data::ServerDataStore store(serverDataDir);
  std::string relativePath = cacheRelativePath(languageCode, word);

  if (!ignoreCache) {
    auto cached = store.read(relativePath);
    if (cached) {
      try {
        WordInfo info = detail::parseWordInfo(nlohmann::json::parse(*cached));
        if (info.entries.empty()) return std::nullopt;
        return info;
      } catch (const nlohmann::json::exception&) {
        // Corrupt/unreadable cache file - fall through to a live fetch
        // rather than failing a lookup that would otherwise succeed.
      }
    }
  }

  nlohmann::json raw = fetchRawJson(word, languageCode);
  WordInfo info = detail::parseWordInfo(raw);
  if (info.entries.empty()) return std::nullopt;

  try {
    store.write(relativePath, raw.dump());
  } catch (const server_data::ServerDataError&) {
    // Best-effort - a cache write failure shouldn't fail an otherwise-good
    // lookup the caller is about to receive anyway.
  }

  return info;
}

std::optional<WordInfo> FreeDictionaryAPIEntry::fetchFromCache(
    const std::string& word, const std::filesystem::path& serverDataDir,
    const std::string& languageCode) {
  server_data::ServerDataStore store(serverDataDir);
  auto cached = store.read(cacheRelativePath(languageCode, word));
  if (!cached) return std::nullopt;

  try {
    WordInfo info = detail::parseWordInfo(nlohmann::json::parse(*cached));
    if (info.entries.empty()) return std::nullopt;
    return info;
  } catch (const nlohmann::json::exception&) {
    return std::nullopt;
  }
}

bool FreeDictionaryAPIEntry::hasCachedEntry(const std::string& word,
                                             const std::filesystem::path& serverDataDir,
                                             const std::string& languageCode) {
  server_data::ServerDataStore store(serverDataDir);
  return store.exists(cacheRelativePath(languageCode, word));
}

namespace detail {

namespace {

std::vector<std::string> parseStringArray(const nlohmann::json& arr) {
  std::vector<std::string> out;
  if (!arr.is_array()) return out;
  out.reserve(arr.size());
  for (const auto& item : arr) {
    if (item.is_string()) out.push_back(item.get<std::string>());
  }
  return out;
}

Sense parseSense(const nlohmann::json& s) {
  Sense sense;
  sense.definition = s.value("definition", "");
  sense.tags = parseStringArray(s.value("tags", nlohmann::json::array()));
  sense.examples = parseStringArray(s.value("examples", nlohmann::json::array()));

  if (s.contains("quotes") && s["quotes"].is_array()) {
    for (const auto& q : s["quotes"]) {
      Quote quote;
      quote.text = q.value("text", "");
      quote.reference = q.value("reference", "");
      sense.quotes.push_back(std::move(quote));
    }
  }

  sense.synonyms = parseStringArray(s.value("synonyms", nlohmann::json::array()));
  sense.antonyms = parseStringArray(s.value("antonyms", nlohmann::json::array()));

  if (s.contains("subsenses") && s["subsenses"].is_array()) {
    for (const auto& sub : s["subsenses"]) {
      sense.subsenses.push_back(parseSense(sub));
    }
  }

  return sense;
}

Entry parseEntry(const nlohmann::json& e) {
  Entry entry;
  if (e.contains("language")) {
    entry.language.code = e["language"].value("code", "");
    entry.language.name = e["language"].value("name", "");
  }
  entry.partOfSpeech = e.value("partOfSpeech", "");

  if (e.contains("pronunciations") && e["pronunciations"].is_array()) {
    for (const auto& p : e["pronunciations"]) {
      Pronunciation pron;
      pron.type = p.value("type", "");
      pron.text = p.value("text", "");
      pron.tags = parseStringArray(p.value("tags", nlohmann::json::array()));
      entry.pronunciations.push_back(std::move(pron));
    }
  }

  if (e.contains("forms") && e["forms"].is_array()) {
    for (const auto& f : e["forms"]) {
      WordForm form;
      form.word = f.value("word", "");
      form.tags = parseStringArray(f.value("tags", nlohmann::json::array()));
      entry.forms.push_back(std::move(form));
    }
  }

  if (e.contains("senses") && e["senses"].is_array()) {
    for (const auto& s : e["senses"]) {
      entry.senses.push_back(parseSense(s));
    }
  }

  entry.synonyms = parseStringArray(e.value("synonyms", nlohmann::json::array()));
  entry.antonyms = parseStringArray(e.value("antonyms", nlohmann::json::array()));

  return entry;
}

}  // namespace

WordInfo parseWordInfo(const nlohmann::json& data) {
  WordInfo info;
  info.word = data.value("word", "");

  if (data.contains("entries") && data["entries"].is_array()) {
    for (const auto& e : data["entries"]) {
      info.entries.push_back(parseEntry(e));
    }
  }

  if (data.contains("source")) {
    const auto& src = data["source"];
    info.source.url = src.value("url", "");
    if (src.contains("license")) {
      info.source.license.name = src["license"].value("name", "");
      info.source.license.url = src["license"].value("url", "");
    }
  }

  return info;
}

}  // namespace detail

}  // namespace dictionary_utils
