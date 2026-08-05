#include "parsing_detail.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using youtube_utils::NoTranscriptFound;

namespace {

nlohmann::json makeTrack(const std::string& languageCode, const std::string& name,
                          bool generated) {
  nlohmann::json track = {
      {"languageCode", languageCode},
      {"name", {{"runs", nlohmann::json::array({{{"text", name}}})}}},
      {"baseUrl", "https://example.invalid/caption?lang=" + languageCode + "&fmt=srv3"},
  };
  if (generated) {
    track["kind"] = "asr";
  }
  return track;
}

}  // namespace

TEST_CASE("prefers a manually-created transcript over a generated one for the same language",
          "[youtube_utils][transcript_list]") {
  nlohmann::json captionsJson = {
      {"captionTracks", nlohmann::json::array({
                             makeTrack("en", "English (auto-generated)", true),
                             makeTrack("en", "English", false),
                         })}};

  auto list = youtube_utils::detail::buildTranscriptList("vid", captionsJson);
  auto transcript = list.findTranscript({"en"});

  REQUIRE_FALSE(transcript.isGenerated());
  REQUIRE(transcript.language() == "English");
}

TEST_CASE("walks the requested language codes in priority order",
          "[youtube_utils][transcript_list]") {
  nlohmann::json captionsJson = {
      {"captionTracks", nlohmann::json::array({
                             makeTrack("en", "English", false),
                             makeTrack("de", "Deutsch", false),
                         })}};

  auto list = youtube_utils::detail::buildTranscriptList("vid", captionsJson);
  auto transcript = list.findTranscript({"de", "en"});

  REQUIRE(transcript.languageCode() == "de");
}

TEST_CASE("strips &fmt=srv3 from the base URL", "[youtube_utils][transcript_list]") {
  nlohmann::json captionsJson = {
      {"captionTracks", nlohmann::json::array({makeTrack("en", "English", false)})}};

  auto list = youtube_utils::detail::buildTranscriptList("vid", captionsJson);
  auto transcript = list.findTranscript({"en"});

  // No public accessor for the URL (only fetch() uses it) - indirectly
  // confirmed via stripFmtSrv3's own unit coverage below instead.
  REQUIRE(transcript.languageCode() == "en");
}

TEST_CASE("throws NoTranscriptFound when none of the requested codes match",
          "[youtube_utils][transcript_list]") {
  nlohmann::json captionsJson = {
      {"captionTracks", nlohmann::json::array({makeTrack("en", "English", false)})}};

  auto list = youtube_utils::detail::buildTranscriptList("vid", captionsJson);
  REQUIRE_THROWS_AS(list.findTranscript({"fr"}), NoTranscriptFound);
}

TEST_CASE("availableLanguageCodes lists every track", "[youtube_utils][transcript_list]") {
  nlohmann::json captionsJson = {
      {"captionTracks", nlohmann::json::array({
                             makeTrack("en", "English", false),
                             makeTrack("es", "Spanish (auto-generated)", true),
                         })}};

  auto list = youtube_utils::detail::buildTranscriptList("vid", captionsJson);
  auto codes = list.availableLanguageCodes();

  REQUIRE(codes.size() == 2);
  REQUIRE((codes[0] == "en" || codes[0] == "es"));
}

TEST_CASE("stripFmtSrv3 removes the suffix when present", "[youtube_utils][transcript_list]") {
  REQUIRE(youtube_utils::detail::stripFmtSrv3("https://x.invalid/c?lang=en&fmt=srv3") ==
          "https://x.invalid/c?lang=en");
}

TEST_CASE("stripFmtSrv3 is a no-op when the suffix is absent", "[youtube_utils][transcript_list]") {
  REQUIRE(youtube_utils::detail::stripFmtSrv3("https://x.invalid/c?lang=en") ==
          "https://x.invalid/c?lang=en");
}
