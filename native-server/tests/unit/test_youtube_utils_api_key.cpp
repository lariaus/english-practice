#include "parsing_detail.h"

#include <catch2/catch_test_macros.hpp>

using youtube_utils::YouTubeRequestFailed;

TEST_CASE("extracts the INNERTUBE_API_KEY from watch-page HTML", "[youtube_utils][api_key]") {
  std::string html = R"(var x = {"INNERTUBE_API_KEY":"AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8", "y": 1};)";
  REQUIRE(youtube_utils::detail::extractInnertubeApiKey(html, "vid") ==
          "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8");
}

TEST_CASE("tolerates whitespace after the colon", "[youtube_utils][api_key]") {
  std::string html = R"("INNERTUBE_API_KEY":   "abc123-_XYZ")";
  REQUIRE(youtube_utils::detail::extractInnertubeApiKey(html, "vid") == "abc123-_XYZ");
}

TEST_CASE("throws YouTubeRequestFailed when the key is missing", "[youtube_utils][api_key]") {
  std::string html = "<html>no api key here</html>";
  REQUIRE_THROWS_AS(youtube_utils::detail::extractInnertubeApiKey(html, "vid"), YouTubeRequestFailed);
}
