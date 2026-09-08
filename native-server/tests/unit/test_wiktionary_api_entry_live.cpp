// Genuinely hits the real en.wiktionary.org over the network - not mocked,
// same convention as test_https_client.cpp, test_youtube_utils_parity.cpp,
// and test_dictionary_utils_live.cpp (a standalone library's own
// real-network test lives in unit/, not integration/ - integration/ is
// reserved for tests exercising native_server_core's assembled HTTP
// routes, which dictionary_utils isn't wired into yet).

#include "dictionary_utils/wiktionary_api_entry.h"
#include "support/live_retry.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

using dictionary_utils::WiktionaryAPIEntry;
using native_server_test::tryFetchAllowingRateLimit;

namespace {

class TempDir {
 public:
  TempDir() {
    _path = std::filesystem::temp_directory_path() /
            ("wiktionary_api_entry_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(_path);
  }
  ~TempDir() { std::filesystem::remove_all(_path); }

  const std::filesystem::path& path() const { return _path; }

 private:
  std::filesystem::path _path;
};

}  // namespace

TEST_CASE("fetches real US audio and IPA for a common word, and downloads the audio",
          "[wiktionary_utils][live]") {
  TempDir dir;

  auto maybeInfo =
      tryFetchAllowingRateLimit([&] { return WiktionaryAPIEntry::fetch("smart", dir.path()); });
  if (!maybeInfo) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeInfo->has_value());
  const auto& info = **maybeInfo;

  REQUIRE(info.hasPronunciationSection);
  REQUIRE_FALSE(info.audio.empty());
  REQUIRE_FALSE(info.ipa.empty());

  const auto* usAudio = info.usAudio();
  REQUIRE(usAudio != nullptr);
  REQUIRE_FALSE(usAudio->audio.empty());

  std::filesystem::path cachedFile = dir.path() / usAudio->audio;
  REQUIRE(std::filesystem::exists(cachedFile));
  CHECK(std::filesystem::file_size(cachedFile) > 0);
}

TEST_CASE(
    "downloads real US audio when the Commons filename contains spaces and parentheses",
    "[wiktionary_utils][live]") {
  // Regression test: "incredible"'s US audio candidate is
  // "LL-Q1860 (eng)-Naomi Persephone Amethyst (NaomiAmethyst)-incredible.wav" -
  // the imageinfo lookup used to fail for exactly this shape of filename.
  // The raw "|" characters in that request's own "iiprop=url|mime|size"
  // tripped newer NSURL's lenient auto-percent-encoding (see
  // http_client.mm's NSURL URLWithString: call), which also re-escaped the
  // %20/%28/%29 this class had already encoded into the filename -
  // corrupting the title into a double-encoded string Wiktionary's API
  // then rejected as invalid. Plain filenames with no spaces/parens (e.g.
  // "smart"'s "en-us-smart.ogg", covered by the test above) never
  // exercised this, since there was nothing for the bug to double-encode.
  TempDir dir;

  auto maybeInfo = tryFetchAllowingRateLimit(
      [&] { return WiktionaryAPIEntry::fetch("incredible", dir.path()); });
  if (!maybeInfo) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeInfo->has_value());
  const auto& info = **maybeInfo;

  REQUIRE(info.hasPronunciationSection);

  const auto* usAudio = info.usAudio();
  REQUIRE(usAudio != nullptr);
  REQUIRE_FALSE(usAudio->audio.empty());
  CHECK(usAudio->filename.find(' ') != std::string::npos);  // exercises the actual regression

  std::filesystem::path cachedFile = dir.path() / usAudio->audio;
  REQUIRE(std::filesystem::exists(cachedFile));
  CHECK(std::filesystem::file_size(cachedFile) > 0);
}

TEST_CASE("a word absent from Wiktionary entirely is not an error", "[wiktionary_utils][live]") {
  TempDir dir;

  auto maybeInfo = tryFetchAllowingRateLimit(
      [&] { return WiktionaryAPIEntry::fetch("asdkjaslkdjalksjdqwerty", dir.path()); });
  if (!maybeInfo) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }

  CHECK_FALSE(maybeInfo->has_value());
}

TEST_CASE("a word that exists only in another language has no English pronunciation",
          "[wiktionary_utils][live]") {
  TempDir dir;

  auto maybeInfo = tryFetchAllowingRateLimit(
      [&] { return WiktionaryAPIEntry::fetch("Katze", dir.path()); });  // German for "cat"
  if (!maybeInfo) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }

  CHECK_FALSE(maybeInfo->has_value());
}

TEST_CASE("a second fetch reuses the persistent cache instead of hitting the network again",
          "[wiktionary_utils][live]") {
  TempDir dir;

  // Only the first (network) fetch needs the rate-limit escape hatch - the
  // second is the actual thing under test (a pure cache read).
  auto maybeFirst =
      tryFetchAllowingRateLimit([&] { return WiktionaryAPIEntry::fetch("smart", dir.path()); });
  if (!maybeFirst) {
    SUCCEED("Hit Wiktionary's real rate limit (HTTP 429) twice in a row - not a real failure");
    return;
  }
  REQUIRE(maybeFirst->has_value());
  const auto& first = **maybeFirst;

  CHECK(std::filesystem::exists(dir.path() / "dictionaries" / "wiktionaryapi" / "en-smart.json"));

  auto second = WiktionaryAPIEntry::fetch("smart", dir.path());
  REQUIRE(second.has_value());
  const auto* usAudio = second->usAudio();
  REQUIRE(usAudio != nullptr);
  CHECK(usAudio->audio == first.usAudio()->audio);
}
