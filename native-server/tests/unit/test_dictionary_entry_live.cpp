// Genuinely hits the real freedictionaryapi.com and en.wiktionary.org over
// the network, and writes real files to a real temp directory - not
// mocked, same convention as the other *_live tests in this directory (see
// test_dictionary_utils_live.cpp's header comment for why this lives in
// unit/ rather than integration/).

#include "dictionary_utils/dictionary_entry.h"
#include "support/live_retry.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <server_data/server_data_store.h>

#include <cstdlib>
#include <filesystem>
#include <optional>

using dictionary_utils::DictionaryEntry;
using native_server_test::tryFetchAllowingRateLimit;

namespace {

class TempDir {
 public:
  TempDir() {
    _path = std::filesystem::temp_directory_path() /
            ("dictionary_entry_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(_path);
  }
  ~TempDir() { std::filesystem::remove_all(_path); }

  const std::filesystem::path& path() const { return _path; }

 private:
  std::filesystem::path _path;
};

}  // namespace

TEST_CASE("fetches, merges, and caches a real word end to end", "[dictionary_entry][live]") {
  TempDir dir;

  auto maybeResult =
      tryFetchAllowingRateLimit([&] { return DictionaryEntry::fetch("smart", dir.path()); });
  if (!maybeResult) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  const auto& result = *maybeResult;

  REQUIRE(result.has_value());
  CHECK(result->word == "smart");
  REQUIRE_FALSE(result->meanings.empty());

  const auto* us = result->usPhonetics();
  REQUIRE(us != nullptr);
  REQUIRE_FALSE(us->audio.empty());

  std::filesystem::path cachedFile = dir.path() / us->audio;
  REQUIRE(std::filesystem::exists(cachedFile));
  CHECK(std::filesystem::file_size(cachedFile) > 0);
}

TEST_CASE("a second fetch reuses the cached file instead of failing", "[dictionary_entry][live]") {
  TempDir dir;

  // Only the first (network) fetch needs the rate-limit escape hatch - the
  // second is the actual thing under test (a pure cache read).
  auto maybeFirst =
      tryFetchAllowingRateLimit([&] { return DictionaryEntry::fetch("smart", dir.path()); });
  if (!maybeFirst) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  const auto& first = *maybeFirst;
  REQUIRE(first.has_value());
  auto second = DictionaryEntry::fetch("smart", dir.path());
  REQUIRE(second.has_value());

  REQUIRE(first->usPhonetics() != nullptr);
  REQUIRE(second->usPhonetics() != nullptr);
  CHECK(first->usPhonetics()->audio == second->usPhonetics()->audio);
}

TEST_CASE("ignoreCache forces a real end-to-end refresh, redownloading audio too",
          "[dictionary_entry][live]") {
  TempDir dir;

  auto maybeFirst =
      tryFetchAllowingRateLimit([&] { return DictionaryEntry::fetch("smart", dir.path()); });
  if (!maybeFirst) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  const auto& first = *maybeFirst;
  REQUIRE(first.has_value());
  REQUIRE(first->usPhonetics() != nullptr);
  std::filesystem::path cachedFile = dir.path() / first->usPhonetics()->audio;
  auto firstWriteTime = std::filesystem::last_write_time(cachedFile);

  auto maybeSecond = tryFetchAllowingRateLimit([&] {
    return DictionaryEntry::fetch("smart", dir.path(), "en", /*fastFetch=*/false,
                                   /*ignoreCache=*/true);
  });
  if (!maybeSecond) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeSecond->has_value());
  auto secondWriteTime = std::filesystem::last_write_time(cachedFile);

  // Catch2 can't stringify std::filesystem::file_time_type directly on
  // this toolchain (a __int128 duration ambiguity in its own headers) -
  // pre-compute the comparison to a plain bool instead of letting
  // CHECK(a >= b) try to decompose/print the operands themselves.
  bool notOlderThanBefore = secondWriteTime >= firstWriteTime;
  CHECK(notOlderThanBefore);
}

TEST_CASE("a word absent from both sources returns nullopt, not an error",
          "[dictionary_entry][live]") {
  TempDir dir;

  auto maybeResult = tryFetchAllowingRateLimit(
      [&] { return DictionaryEntry::fetch("asdkjaslkdjalksjdqwerty", dir.path()); });
  if (!maybeResult) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  CHECK_FALSE(maybeResult->has_value());
}

TEST_CASE("input is trimmed and lowercased before lookup", "[dictionary_entry][live]") {
  TempDir dir;

  auto maybeResult =
      tryFetchAllowingRateLimit([&] { return DictionaryEntry::fetch("  SMART  ", dir.path()); });
  if (!maybeResult) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeResult->has_value());
  CHECK((*maybeResult)->word == "smart");
}

TEST_CASE("fastFetch only hits FreeDictionaryAPIEntry and never writes the cache",
          "[dictionary_entry][fast_fetch]") {
  // Only exercises freedictionaryapi.com, not Wiktionary - deliberately
  // avoids the Wiktionary-rate-limit risk the other live tests in this
  // file carry.
  TempDir dir;

  auto result = DictionaryEntry::fetch("smart", dir.path(), "en", /*fastFetch=*/true);

  REQUIRE(result.has_value());
  CHECK(result->word == "smart");
  REQUIRE_FALSE(result->meanings.empty());
  // No Wiktionary call means no real audio, ever, in fastFetch mode.
  for (const auto& p : result->phonetics) {
    CHECK(p.audio.empty());
  }

  server_data::ServerDataStore store(dir.path());
  CHECK_FALSE(store.exists("dictionaries/entries/en-smart.json"));
}

TEST_CASE("a cached entry is served with zero network calls",
          "[dictionary_entry][cache]") {
  // Deterministic, no live network required despite this file's usual
  // convention - the whole point: pre-seed a cache file for a word that
  // does not exist in any real dictionary, then confirm fetch() returns
  // the fabricated content instead of nullopt. That content could only
  // have come from the cache, since no real API would ever return it live.
  TempDir dir;
  server_data::ServerDataStore store(dir.path());
  store.write("dictionaries/entries/en-definitelynotarealword12345.json",
              nlohmann::json{{"word", "definitelynotarealword12345"},
                             {"phonetics", nlohmann::json::array()},
                             {"meanings", nlohmann::json::array()},
                             {"sourceUrl", "fabricated"},
                             {"license", "fabricated"}}
                  .dump());

  auto result = DictionaryEntry::fetch("definitelynotarealword12345", dir.path());

  REQUIRE(result.has_value());
  CHECK(result->sourceUrl == "fabricated");
}
