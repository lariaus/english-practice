// Genuinely hits the real freedictionaryapi.com over the network - not
// mocked, same convention as test_https_client.cpp and
// test_youtube_utils_parity.cpp (a standalone library's own real-network
// test lives in unit/, not integration/ - integration/ is reserved for
// tests that exercise native_server_core's assembled HTTP routes, which
// dictionary_utils isn't wired into yet). Unlike the old dictionaryapi.dev
// scraping approach (see docs/dictionary-spec.md), this API is actively
// maintained and keyless, so this is expected to actually pass in normal
// conditions - but it still depends on a real external service being
// reachable.

#include "dictionary_utils/free_dictionary_api_entry.h"
#include "support/live_retry.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

using dictionary_utils::FreeDictionaryAPIEntry;
using native_server_test::tryFetchAllowingRateLimit;

namespace {

class TempDir {
 public:
  TempDir() {
    _path = std::filesystem::temp_directory_path() /
            ("free_dictionary_api_entry_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(_path);
  }
  ~TempDir() { std::filesystem::remove_all(_path); }

  const std::filesystem::path& path() const { return _path; }

 private:
  std::filesystem::path _path;
};

}  // namespace

TEST_CASE("fetches real structured data for a common word", "[dictionary_utils][live]") {
  TempDir dir;

  auto maybeInfo =
      tryFetchAllowingRateLimit([&] { return FreeDictionaryAPIEntry::fetch("smart", dir.path()); });
  if (!maybeInfo) {
    SUCCEED("Hit FreeDictionaryAPI's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeInfo->has_value());
  const auto& info = **maybeInfo;

  CHECK(info.word == "smart");
  REQUIRE_FALSE(info.entries.empty());

  bool hasGeneralAmerican = false;
  for (const auto& e : info.entries) {
    for (const auto& p : e.pronunciations) {
      for (const auto& tag : p.tags) {
        if (tag == "General American") hasGeneralAmerican = true;
      }
    }
  }
  CHECK(hasGeneralAmerican);
}

TEST_CASE("a word absent from the dictionary is not an error", "[dictionary_utils][live]") {
  TempDir dir;

  auto maybeInfo = tryFetchAllowingRateLimit(
      [&] { return FreeDictionaryAPIEntry::fetch("asdkjaslkdjalksjdqwerty", dir.path()); });
  if (!maybeInfo) {
    SUCCEED("Hit FreeDictionaryAPI's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }

  CHECK_FALSE(maybeInfo->has_value());
}

TEST_CASE("a second fetch reuses the persistent cache instead of hitting the network again",
          "[dictionary_utils][live]") {
  TempDir dir;

  // Only the first (network) fetch needs the rate-limit escape hatch - the
  // second is the actual thing under test (a pure cache read).
  auto maybeFirst =
      tryFetchAllowingRateLimit([&] { return FreeDictionaryAPIEntry::fetch("smart", dir.path()); });
  if (!maybeFirst) {
    SUCCEED("Hit FreeDictionaryAPI's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeFirst->has_value());
  const auto& first = **maybeFirst;

  CHECK(std::filesystem::exists(dir.path() / "dictionaries" / "freedictionaryapi" /
                                 "en-smart.json"));

  auto second = FreeDictionaryAPIEntry::fetch("smart", dir.path());
  REQUIRE(second.has_value());
  CHECK(second->word == first.word);
}
