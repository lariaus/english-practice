// GET /dictionary tests genuinely hit the real Free Dictionary API +
// Wiktionary through the full assembled route, end to end - not mocked,
// same philosophy as test_subtitles_route.cpp. POST /dictionary/
// us-audio-words is deliberately cache-only (see its own doc comment in
// dictionary_route.cpp) - those tests below are fully offline instead,
// pre-seeding a fake cache file directly rather than hitting anything real.

#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using native_server_test::fixturesDir;
using native_server_test::kTestHost;
using native_server_test::makeTempDir;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18085;

// Same rate-limit escape hatch as the dictionary_utils *_live.cpp unit
// tests (see tests/support/live_retry.h) - DictionaryEntry::fetch() now
// propagates a real Wiktionary failure rather than silently degrading, so
// a genuine HTTP 429 from Wiktionary surfaces here as a 502 whose body
// echoes "429", rather than as a thrown C++ exception. That HTTP-shaped
// failure mode is specific to this route-level test, hence the duplicated
// (not shared) retry logic here rather than reusing live_retry.h directly.
httplib::Result getAllowingRateLimit(httplib::Client& client, const httplib::Params& params) {
  for (int attempt = 1; attempt <= 2; ++attempt) {
    auto res = client.Get("/dictionary", params, httplib::Headers{});
    bool rateLimited = res && res->status == 502 && res->body.find("429") != std::string::npos;
    if (!rateLimited || attempt == 2) return res;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return client.Get("/dictionary", params, httplib::Headers{});
}

bool isRateLimited(const httplib::Result& res) {
  return res && res->status == 502 && res->body.find("429") != std::string::npos;
}

}  // namespace

TEST_CASE("GET /dictionary returns real data and caches audio, reachable via /server_data",
          "[integration][dictionary]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = getAllowingRateLimit(client, httplib::Params{{"word", "smart"}});
  REQUIRE(res);
  if (isRateLimited(res)) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    std::filesystem::remove_all(dataDir);
    return;
  }
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("word") == "smart");
  REQUIRE_FALSE(body.at("meanings").empty());

  const auto& phonetics = body.at("phonetics");
  REQUIRE(phonetics.is_array());
  auto usEntry = std::find_if(phonetics.begin(), phonetics.end(),
                               [](const nlohmann::json& p) { return p.at("label") == "US"; });
  REQUIRE(usEntry != phonetics.end());
  std::string audioPath = usEntry->at("audio").get<std::string>();
  REQUIRE_FALSE(audioPath.empty());

  // Round-trip: the audio path returned above must be independently
  // fetchable through the static /server_data mount, with real bytes -
  // this is what actually makes cached pronunciation audio reachable at
  // all from the browser.
  auto audioRes = client.Get(("/server_data/" + audioPath).c_str());
  REQUIRE(audioRes);
  REQUIRE(audioRes->status == 200);
  REQUIRE_FALSE(audioRes->body.empty());

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /dictionary without a word parameter returns a 400 JSON error",
          "[integration][dictionary]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/dictionary");
  REQUIRE(res);
  REQUIRE(res->status == 400);
  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("error").is_string());

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /dictionary for a word absent from both sources returns 404",
          "[integration][dictionary]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = getAllowingRateLimit(client, httplib::Params{{"word", "asdkjaslkdjalksjdqwerty"}});
  REQUIRE(res);
  if (isRateLimited(res)) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    std::filesystem::remove_all(dataDir);
    return;
  }
  REQUIRE(res->status == 404);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /dictionary defaults lang to 'en' when omitted", "[integration][dictionary]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = getAllowingRateLimit(client, httplib::Params{{"word", "smart"}});
  REQUIRE(res);
  if (isRateLimited(res)) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    std::filesystem::remove_all(dataDir);
    return;
  }
  REQUIRE(res->status == 200);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /dictionary?fast=true skips Wiktionary (no audio) and never caches the entry",
          "[integration][dictionary]") {
  // Only exercises freedictionaryapi.com, not Wiktionary - deliberately
  // avoids the Wiktionary-rate-limit risk the main round-trip test carries.
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/dictionary", httplib::Params{{"word", "smart"}, {"fast", "true"}},
                         httplib::Headers{});
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("word") == "smart");
  REQUIRE_FALSE(body.at("meanings").empty());
  for (const auto& p : body.at("phonetics")) {
    CHECK(p.at("audio").get<std::string>().empty());
  }

  CHECK_FALSE(std::filesystem::exists(dataDir / "server_data" / "dictionaries" / "entries" /
                                       "en-smart.json"));

  std::filesystem::remove_all(dataDir);
}

namespace {

// Writes a fake pre-cached Wiktionary entry directly to disk, in the exact
// shape wiktionaryWordInfoToJson() itself produces - lets these tests
// exercise POST /dictionary/us-audio-words entirely offline (it's
// deliberately cache-only - see dictionary_route.cpp/
// wiktionary_api_entry.cpp's own hasCachedUsAudio), with no real network
// calls or rate-limit risk at all, unlike this file's other tests.
void writeFakeWiktionaryCache(const std::filesystem::path& dataDir, const std::string& word,
                               bool hasUsAudio) {
  std::filesystem::path path =
      dataDir / "server_data" / "dictionaries" / "wiktionaryapi" / ("en-" + word + ".json");
  std::filesystem::create_directories(path.parent_path());

  nlohmann::json audio = nlohmann::json::array();
  if (hasUsAudio) {
    audio.push_back({{"filename", "en-us-" + word + ".ogg"},
                      {"audio", "dictionaries/wiktionaryapi/en-us-" + word + ".ogg"},
                      {"mimeType", "audio/ogg"},
                      {"durationSeconds", 1.0},
                      {"sizeBytes", 1234},
                      {"accents", nlohmann::json::array({"US"})}});
  }

  nlohmann::json entry = {{"word", word},
                           {"audio", audio},
                           {"ipa", nlohmann::json::array()},
                           {"hasPronunciationSection", true}};

  std::ofstream file(path, std::ios::binary);
  file << entry.dump();
}

}  // namespace

TEST_CASE("POST /dictionary/us-audio-words returns only words with cached US audio, entirely "
          "offline",
          "[integration][dictionary][us-audio-words]") {
  auto dataDir = makeTempDir();
  writeFakeWiktionaryCache(dataDir, "hello", /*hasUsAudio=*/true);
  writeFakeWiktionaryCache(dataDir, "goodbye", /*hasUsAudio=*/false);
  // "unseen" deliberately has no cache file at all.
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  nlohmann::json requestBody = {{"words", {"hello", "goodbye", "unseen"}}};
  auto res = client.Post("/dictionary/us-audio-words", requestBody.dump(), "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("words") == nlohmann::json::array({"hello"}));

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("POST /dictionary/us-audio-words normalizes words the same way DictionaryEntry::fetch() "
          "would (trim + lowercase)",
          "[integration][dictionary][us-audio-words]") {
  auto dataDir = makeTempDir();
  writeFakeWiktionaryCache(dataDir, "hello", /*hasUsAudio=*/true);
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  nlohmann::json requestBody = {{"words", {"  Hello  "}}};
  auto res = client.Post("/dictionary/us-audio-words", requestBody.dump(), "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("words") == nlohmann::json::array({"hello"}));

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("POST /dictionary/us-audio-words returns an empty list for an empty/missing "
          "server_data, never an error",
          "[integration][dictionary][us-audio-words]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  nlohmann::json requestBody = {{"words", {"anything", "goes"}}};
  auto res = client.Post("/dictionary/us-audio-words", requestBody.dump(), "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  CHECK(nlohmann::json::parse(res->body).at("words") == nlohmann::json::array());

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("POST /dictionary/us-audio-words with a malformed body returns 400",
          "[integration][dictionary][us-audio-words]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Post("/dictionary/us-audio-words", "not json", "application/json");
  REQUIRE(res);
  CHECK(res->status == 400);

  auto res2 = client.Post("/dictionary/us-audio-words", "{}", "application/json");
  REQUIRE(res2);
  CHECK(res2->status == 400);

  std::filesystem::remove_all(dataDir);
}
