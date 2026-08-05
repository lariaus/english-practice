#include "parsing_detail.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using youtube_utils::TranscriptsDisabled;

TEST_CASE("extracts the captionTracks renderer when present", "[youtube_utils][captions_json]") {
  nlohmann::json data = {
      {"captions",
       {{"playerCaptionsTracklistRenderer",
         {{"captionTracks", nlohmann::json::array({{{"languageCode", "en"}}})}}}}}};

  auto captions = youtube_utils::detail::extractCaptionsJson(data, "vid");
  REQUIRE(captions.at("captionTracks").size() == 1);
}

TEST_CASE("throws TranscriptsDisabled when 'captions' is missing", "[youtube_utils][captions_json]") {
  nlohmann::json data = nlohmann::json::object();
  REQUIRE_THROWS_AS(youtube_utils::detail::extractCaptionsJson(data, "vid"), TranscriptsDisabled);
}

TEST_CASE("throws TranscriptsDisabled when captionTracks is missing",
          "[youtube_utils][captions_json]") {
  nlohmann::json data = {{"captions", {{"playerCaptionsTracklistRenderer", nlohmann::json::object()}}}};
  REQUIRE_THROWS_AS(youtube_utils::detail::extractCaptionsJson(data, "vid"), TranscriptsDisabled);
}
