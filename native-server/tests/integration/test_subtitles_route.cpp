#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

using native_server_test::kTestHost;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18083;
}

TEST_CASE("GET /subtitles returns real cues for a bare video ID", "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/subtitles", httplib::Params{{"url", "7porh0HB0HU"}, {"lang", "en"}}, httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("videoId") == "7porh0HB0HU");
  REQUIRE(body.at("languageCode") == "en");
  REQUIRE(body.at("cues").is_array());
  REQUIRE(body.at("cues").size() > 0);
  REQUIRE(body.at("cues")[0].at("text") == "In 1972, the actor Anthony Hopkins was");
}

TEST_CASE("GET /subtitles accepts a full YouTube URL, not just a bare ID",
          "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get(
      "/subtitles",
      httplib::Params{{"url", "https://www.youtube.com/watch?v=7porh0HB0HU"}, {"lang", "en"}}, httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("videoId") == "7porh0HB0HU");
}

TEST_CASE("GET /subtitles defaults lang to 'en' when omitted", "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/subtitles", httplib::Params{{"url", "7porh0HB0HU"}}, httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("languageCode") == "en");
}

TEST_CASE("GET /subtitles without a url param returns a 400 JSON error",
          "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/subtitles");
  REQUIRE(res);
  REQUIRE(res->status == 400);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("error").is_string());
}

TEST_CASE("GET /subtitles for an unparseable url returns a 400 JSON error",
          "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/subtitles", httplib::Params{{"url", "not-a-video-id-or-url"}}, httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 400);
}

TEST_CASE("GET /subtitles for an unavailable video returns a 404 JSON error",
          "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  // "aaaaaaaaaaa" is a well-formed (11-char) but non-existent video ID.
  auto res = client.Get("/subtitles", httplib::Params{{"url", "aaaaaaaaaaa"}}, httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 404);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("error") == "Video unavailable or does not exist.");
}

TEST_CASE("GET /subtitles with no matching language returns 404 with availableLanguages",
          "[integration][subtitles]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/subtitles", httplib::Params{{"url", "7porh0HB0HU"}, {"lang", "zz"}}, httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 404);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("error").get<std::string>().find("'zz'") != std::string::npos);
  const auto& available = body.at("availableLanguages");
  REQUIRE(available.is_array());
  REQUIRE(std::find(available.begin(), available.end(), "en") != available.end());
}
