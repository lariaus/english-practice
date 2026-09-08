#include "dictionary_route.h"

#include <dictionary_utils/dictionary_entry.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace native_server {

namespace {

void sendJson(httplib::Response& res, int status, const nlohmann::json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

nlohmann::json phoneticsToJson(const std::vector<dictionary_utils::PhoneticEntry>& phonetics) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& p : phonetics) {
    arr.push_back({{"text", p.text}, {"label", p.label}, {"audio", p.audio}});
  }
  return arr;
}

nlohmann::json meaningsToJson(const std::vector<dictionary_utils::MeaningEntry>& meanings) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& meaning : meanings) {
    nlohmann::json defs = nlohmann::json::array();
    for (const auto& def : meaning.definitions) {
      defs.push_back({{"definition", def.definition},
                       {"examples", def.examples},
                       {"synonyms", def.synonyms},
                       {"antonyms", def.antonyms}});
    }
    arr.push_back({{"partOfSpeech", meaning.partOfSpeech}, {"definitions", defs}});
  }
  return arr;
}

// A direct field-for-field reflection of DictionaryEntry - deliberately not
// narrowed to match the JS webapp's current UI shape, which is a separate,
// JS-side concern (see dictionaryClient.js). usPhonetics() is a derived
// convenience, not serialized - the client recomputes it the same way.
nlohmann::json entryToJson(const dictionary_utils::DictionaryEntry& entry) {
  return {{"word", entry.word},
          {"phonetics", phoneticsToJson(entry.phonetics)},
          {"meanings", meaningsToJson(entry.meanings)},
          {"sourceUrl", entry.sourceUrl},
          {"license", entry.license}};
}

bool isBlank(const std::string& s) {
  return s.find_first_not_of(" \t\n\r") == std::string::npos;
}

bool isTruthy(const std::string& value) {
  return value == "true" || value == "1";
}

}  // namespace

void registerDictionaryRoute(httplib::Server& svr, std::filesystem::path serverDataDir) {
  // Escape hatch for manual/live testing - every dictionary cache layer
  // (whole-entry, WiktionaryAPIEntry, FreeDictionaryAPIEntry) gets bypassed
  // and re-fetched live on every request when set, same as ignoreCache=true
  // on a single DictionaryEntry::fetch call. Read once at route
  // registration time - env vars don't change mid-process.
  bool ignoreCache = std::getenv("DICTIONARY_IGNORE_CACHE") != nullptr;

  svr.Get("/dictionary", [serverDataDir, ignoreCache](const httplib::Request& req,
                                                       httplib::Response& res) {
    std::string word = req.get_param_value("word");
    if (isBlank(word)) {
      sendJson(res, 400, {{"error", "Missing required 'word' query parameter."}});
      return;
    }
    std::string lang = req.has_param("lang") ? req.get_param_value("lang") : "en";
    bool fast = req.has_param("fast") && isTruthy(req.get_param_value("fast"));

    std::optional<dictionary_utils::DictionaryEntry> entry;
    try {
      entry = dictionary_utils::DictionaryEntry::fetch(word, serverDataDir, lang, fast, ignoreCache);
    } catch (const dictionary_utils::DictionaryEntryError& e) {
      sendJson(res, 502,
               {{"error", std::string("Could not look up dictionary entry: ") + e.what()}});
      return;
    }

    if (!entry) {
      sendJson(res, 404, {{"error", "No dictionary entry found for \"" + word + "\"."}});
      return;
    }

    sendJson(res, 200, entryToJson(*entry));
  });

  // Bulk vocabulary filter for ShadowLoopMode (see
  // docs/shadow-loop-mode-spec.md) - given a word list (e.g. every unique
  // word in a YT video's transcript), returns just the subset that already
  // has a cached, real US audio recording. Deliberately cache-only, never
  // touching the network for any word - checking dozens/hundreds of
  // transcript words live would be slow and risks Wiktionary's own rate
  // limiting, and this is meant to be near-instant. Words are normalized
  // the same way DictionaryEntry::fetch() itself would (trim + lowercase +
  // the same small proper-noun exceptions), so a transcript's raw
  // capitalization/whitespace doesn't affect matching.
  svr.Post("/dictionary/us-audio-words",
           [serverDataDir](const httplib::Request& req, httplib::Response& res) {
             nlohmann::json body;
             try {
               body = nlohmann::json::parse(req.body);
             } catch (const nlohmann::json::exception&) {
               sendJson(res, 400, {{"error", "Malformed JSON body."}});
               return;
             }
             if (!body.contains("words") || !body["words"].is_array()) {
               sendJson(res, 400, {{"error", "Missing required 'words' array in JSON body."}});
               return;
             }

             std::string lang = req.has_param("lang") ? req.get_param_value("lang") : "en";

             nlohmann::json matches = nlohmann::json::array();
             for (const auto& rawWord : body["words"]) {
               if (!rawWord.is_string()) continue;
               std::string normalized =
                   dictionary_utils::DictionaryEntry::normalizeWord(rawWord.get<std::string>());
               if (normalized.empty()) continue;
               auto entry =
                   dictionary_utils::DictionaryEntry::fetchFromCache(normalized, serverDataDir, lang);
               if (entry && entry->hasEnUsAudio()) {
                 matches.push_back(normalized);
               }
             }

             sendJson(res, 200, {{"words", matches}});
           });
}

}  // namespace native_server
