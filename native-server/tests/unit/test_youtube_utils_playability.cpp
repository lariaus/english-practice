#include "parsing_detail.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using youtube_utils::VideoUnavailable;
using youtube_utils::YouTubeRequestFailed;

TEST_CASE("status OK passes without throwing", "[youtube_utils][playability]") {
  nlohmann::json data = {{"playabilityStatus", {{"status", "OK"}}}};
  REQUIRE_NOTHROW(youtube_utils::detail::assertPlayability(data, "vid"));
}

TEST_CASE("missing playabilityStatus passes without throwing", "[youtube_utils][playability]") {
  nlohmann::json data = nlohmann::json::object();
  REQUIRE_NOTHROW(youtube_utils::detail::assertPlayability(data, "vid"));
}

TEST_CASE("ERROR + 'This video is unavailable' throws VideoUnavailable",
          "[youtube_utils][playability]") {
  nlohmann::json data = {
      {"playabilityStatus", {{"status", "ERROR"}, {"reason", "This video is unavailable"}}}};
  REQUIRE_THROWS_AS(youtube_utils::detail::assertPlayability(data, "vid"), VideoUnavailable);
}

TEST_CASE("any other non-OK status throws the generic YouTubeRequestFailed",
          "[youtube_utils][playability]") {
  nlohmann::json data = {{"playabilityStatus",
                           {{"status", "LOGIN_REQUIRED"},
                            {"reason", "Sign in to confirm you\xe2\x80\x99re not a bot"}}}};
  REQUIRE_THROWS_AS(youtube_utils::detail::assertPlayability(data, "vid"), YouTubeRequestFailed);
}
