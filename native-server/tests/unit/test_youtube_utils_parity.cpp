#include <youtube_utils/youtube_transcript_api.h>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

// Reference data is the real Python youtube_transcript_api's exact output
// for these video IDs - proving the live C++ path produces the same
// result today, not a frozen/mocked one. Generated on demand (not
// committed - see .gitignore/YOUTUBE_UTILS_PARITY_FIXTURES_DIR) by
// shelling out to generate_youtube_parity_fixture.py whenever the cached
// file is missing, or unconditionally if REGENERATE_PYTHON_REF_DATA is
// set. See docs/migration-to-offline-app/step-4.md.
nlohmann::json loadOrRegenerateFixture(const std::string& videoId) {
  std::filesystem::path dir(YOUTUBE_UTILS_PARITY_FIXTURES_DIR);
  std::filesystem::path path = dir / (videoId + ".json");

  bool forceRegenerate = std::getenv("REGENERATE_PYTHON_REF_DATA") != nullptr;
  if (forceRegenerate || !std::filesystem::exists(path)) {
    std::filesystem::create_directories(dir);
    // Deliberately a bare "python3" (not a configure-time-discovered
    // path) - inherits whatever's on PATH when the test actually runs,
    // which is what respects an activated venv correctly (configure time
    // and run time aren't guaranteed to be the same shell/environment).
    std::string command = "python3 \"" + std::string(YOUTUBE_UTILS_GENERATE_FIXTURE_SCRIPT) +
                           "\" " + videoId + " en \"" + path.string() + "\"";
    int status = std::system(command.c_str());
    REQUIRE(status == 0);
  }

  std::ifstream file(path);
  REQUIRE(file.is_open());
  nlohmann::json data;
  file >> data;
  return data;
}

void checkParity(const std::string& videoId) {
  nlohmann::json expected = loadOrRegenerateFixture(videoId);

  youtube_utils::YouTubeTranscriptApi api;
  auto list = api.list(videoId);
  auto transcript = list.findTranscript({"en"});
  auto fetched = transcript.fetch();

  REQUIRE(transcript.language() == expected.at("language").get<std::string>());
  REQUIRE(transcript.languageCode() == expected.at("languageCode").get<std::string>());
  REQUIRE(transcript.isGenerated() == expected.at("isGenerated").get<bool>());

  const auto& expectedCues = expected.at("cues");
  REQUIRE(fetched.snippets.size() == expectedCues.size());

  for (std::size_t i = 0; i < fetched.snippets.size(); ++i) {
    const auto& snippet = fetched.snippets[i];
    const auto& expectedCue = expectedCues[i];
    REQUIRE(snippet.text == expectedCue.at("text").get<std::string>());
    REQUIRE(snippet.start == expectedCue.at("start").get<double>());
    REQUIRE(snippet.duration == expectedCue.at("duration").get<double>());
  }
}

}  // namespace

TEST_CASE("matches real Python youtube_transcript_api exactly for video 7porh0HB0HU",
          "[youtube_utils][parity]") {
  checkParity("7porh0HB0HU");
}

TEST_CASE("matches real Python youtube_transcript_api exactly for video E8URlWsXR_Q",
          "[youtube_utils][parity]") {
  checkParity("E8URlWsXR_Q");
}

TEST_CASE("matches real Python youtube_transcript_api exactly for video tqO94YKFRpk",
          "[youtube_utils][parity]") {
  checkParity("tqO94YKFRpk");
}
