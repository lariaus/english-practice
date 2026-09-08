#include "parsing_detail.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using dictionary_utils::WordInfo;

namespace {

// Trimmed-down but real shape, captured from
// https://freedictionaryapi.com/api/v1/entries/en/smart - covers
// pronunciations (with accent tags), forms, a sense with examples/quotes/
// synonyms/antonyms, and entry-level synonyms/antonyms.
constexpr const char* kSmartFixture = R"({
  "word": "smart",
  "entries": [
    {
      "language": {"code": "en", "name": "English"},
      "partOfSpeech": "verb",
      "pronunciations": [
        {"type": "ipa", "text": "/smɑɹt/", "tags": ["General American"]},
        {"type": "ipa", "text": "/smɑːt/", "tags": ["Received Pronunciation"]}
      ],
      "forms": [
        {"word": "smarts", "tags": ["present", "singular", "third-person"]},
        {"word": "smarted", "tags": ["past"]}
      ],
      "senses": [
        {
          "definition": "(intransitive) To hurt or sting.",
          "tags": ["intransitive"],
          "examples": ["After being hit with a pitch, the batter exclaimed \"Ouch, my arm smarts!\""],
          "quotes": [
            {"text": "He moved convulsively...", "reference": "1897, Bram Stoker, Dracula"}
          ],
          "synonyms": ["sting"],
          "antonyms": [],
          "subsenses": []
        }
      ],
      "synonyms": ["clever", "intelligent"],
      "antonyms": ["dumb"]
    },
    {
      "language": {"code": "en", "name": "English"},
      "partOfSpeech": "adjective",
      "pronunciations": [],
      "forms": [],
      "senses": [],
      "synonyms": [],
      "antonyms": []
    }
  ],
  "source": {
    "url": "https://en.wiktionary.org/wiki/smart",
    "license": {"name": "CC BY-SA 4.0", "url": "https://creativecommons.org/licenses/by-sa/4.0/"}
  }
})";

// A word not present in the dictionary - the real API responds HTTP 200
// with an empty `entries` array, not an error status.
constexpr const char* kNotFoundFixture = R"({
  "word": "asdkjaslkdjalksjd",
  "entries": [],
  "source": {
    "url": "https://en.wiktionary.org",
    "license": {"name": "CC BY-SA 4.0", "url": "https://creativecommons.org/licenses/by-sa/4.0/"}
  }
})";

}  // namespace

TEST_CASE("parses word and every entry's part of speech", "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kSmartFixture));

  REQUIRE(info.word == "smart");
  REQUIRE_FALSE(info.entries.empty());
  REQUIRE(info.entries.size() == 2);
  CHECK(info.entries[0].partOfSpeech == "verb");
  CHECK(info.entries[1].partOfSpeech == "adjective");
}

TEST_CASE("parses pronunciations with their accent tags", "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kSmartFixture));

  const auto& pronunciations = info.entries[0].pronunciations;
  REQUIRE(pronunciations.size() == 2);
  CHECK(pronunciations[0].type == "ipa");
  CHECK(pronunciations[0].tags == std::vector<std::string>{"General American"});
  CHECK(pronunciations[1].tags == std::vector<std::string>{"Received Pronunciation"});
}

TEST_CASE("parses word forms", "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kSmartFixture));

  const auto& forms = info.entries[0].forms;
  REQUIRE(forms.size() == 2);
  CHECK(forms[0].word == "smarts");
  CHECK(forms[0].tags == std::vector<std::string>({"present", "singular", "third-person"}));
}

TEST_CASE("parses a sense's definition, examples, quotes, synonyms and antonyms",
          "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kSmartFixture));

  REQUIRE(info.entries[0].senses.size() == 1);
  const auto& sense = info.entries[0].senses[0];
  CHECK(sense.definition == "(intransitive) To hurt or sting.");
  CHECK(sense.examples.size() == 1);
  REQUIRE(sense.quotes.size() == 1);
  CHECK(sense.quotes[0].reference == "1897, Bram Stoker, Dracula");
  CHECK(sense.synonyms == std::vector<std::string>{"sting"});
  CHECK(sense.antonyms.empty());
  CHECK(sense.subsenses.empty());
}

TEST_CASE("parses entry-level synonyms and antonyms separately from sense-level ones",
          "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kSmartFixture));

  CHECK(info.entries[0].synonyms == std::vector<std::string>({"clever", "intelligent"}));
  CHECK(info.entries[0].antonyms == std::vector<std::string>{"dumb"});
}

TEST_CASE("parses the source URL and license", "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kSmartFixture));

  CHECK(info.source.url == "https://en.wiktionary.org/wiki/smart");
  CHECK(info.source.license.name == "CC BY-SA 4.0");
}

TEST_CASE("a word absent from the dictionary parses to an empty entries list, not an error",
          "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(kNotFoundFixture));

  CHECK(info.word == "asdkjaslkdjalksjd");
  CHECK(info.entries.empty());
}

TEST_CASE("missing optional fields default to empty rather than throwing",
          "[dictionary_utils][parsing]") {
  WordInfo info = dictionary_utils::detail::parseWordInfo(nlohmann::json::parse(R"({"word": "x"})"));

  CHECK(info.word == "x");
  CHECK(info.entries.empty());
  CHECK(info.source.url.empty());
}
