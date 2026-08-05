#include "youtube_utils/youtube_transcript_api.h"

#include "html_unescape.h"
#include "parsing_detail.h"

#include <https_client/http_client.h>
#include <nlohmann/json.hpp>

#include <regex>
#include <utility>

namespace youtube_utils {

namespace {

constexpr const char* kWatchUrlPrefix = "https://www.youtube.com/watch?v=";
constexpr const char* kInnertubeApiUrlPrefix = "https://www.youtube.com/youtubei/v1/player?key=";

std::string fetchVideoHtml(https_client::HttpClient& client, const std::string& videoId) {
  https_client::HttpResponse res;
  try {
    res = client.get(kWatchUrlPrefix + videoId, {{"Accept-Language", "en-US"}});
  } catch (const https_client::HttpError& e) {
    throw YouTubeRequestFailed(std::string("Could not fetch video page: ") + e.what());
  }
  if (res.statusCode != 200) {
    throw YouTubeRequestFailed("Unexpected status fetching video page: " +
                                std::to_string(res.statusCode));
  }
  return detail::htmlUnescape(res.body);
}

nlohmann::json fetchInnertubeData(https_client::HttpClient& client, const std::string& videoId,
                                   const std::string& apiKey) {
  nlohmann::json requestBody = {
      {"context", {{"client", {{"clientName", "ANDROID"}, {"clientVersion", "20.10.38"}}}}},
      {"videoId", videoId},
  };

  https_client::HttpResponse res;
  try {
    res = client.post(kInnertubeApiUrlPrefix + apiKey, {{"Content-Type", "application/json"}},
                       requestBody.dump());
  } catch (const https_client::HttpError& e) {
    throw YouTubeRequestFailed(std::string("Could not fetch innertube data: ") + e.what());
  }
  if (res.statusCode != 200) {
    throw YouTubeRequestFailed("Unexpected status from innertube API: " +
                                std::to_string(res.statusCode));
  }

  try {
    return nlohmann::json::parse(res.body);
  } catch (const nlohmann::json::parse_error& e) {
    throw YouTubeRequestFailed(std::string("Could not parse innertube response: ") + e.what());
  }
}

}  // namespace

namespace detail {

std::string extractInnertubeApiKey(const std::string& html, const std::string& videoId) {
  static const std::regex kApiKeyRegex(R"re("INNERTUBE_API_KEY":\s*"([a-zA-Z0-9_-]+)")re");
  std::smatch match;
  if (std::regex_search(html, match, kApiKeyRegex)) {
    return match[1].str();
  }
  throw YouTubeRequestFailed("Could not extract INNERTUBE_API_KEY for video: " + videoId);
}

// Only distinguishes what native-exp-server/server.py itself distinguishes
// today (VideoUnavailable) - everything else non-OK collapses into the
// generic YouTubeRequestFailed, matching server.py's own `except Exception`
// catch-all. See docs/migration-to-offline-app/step-4.md.
void assertPlayability(const nlohmann::json& innertubeData, const std::string& videoId) {
  auto statusIt = innertubeData.find("playabilityStatus");
  if (statusIt == innertubeData.end()) {
    return;
  }

  std::string status = statusIt->value("status", "");
  if (status.empty() || status == "OK") {
    return;
  }

  std::string reason = statusIt->value("reason", "");
  if (status == "ERROR" && reason == "This video is unavailable") {
    throw VideoUnavailable(videoId);
  }

  throw YouTubeRequestFailed("Video is not playable (status=" + status + ", reason=" + reason +
                              "): " + videoId);
}

nlohmann::json extractCaptionsJson(const nlohmann::json& innertubeData,
                                    const std::string& videoId) {
  auto captionsIt = innertubeData.find("captions");
  if (captionsIt == innertubeData.end()) {
    throw TranscriptsDisabled(videoId);
  }
  auto rendererIt = captionsIt->find("playerCaptionsTracklistRenderer");
  if (rendererIt == captionsIt->end() || !rendererIt->contains("captionTracks")) {
    throw TranscriptsDisabled(videoId);
  }
  return *rendererIt;
}

std::string stripFmtSrv3(std::string url) {
  static const std::string kSuffix = "&fmt=srv3";
  auto pos = url.find(kSuffix);
  if (pos != std::string::npos) {
    url.erase(pos, kSuffix.size());
  }
  return url;
}

std::string extractTrackDisplayName(const nlohmann::json& track, const std::string& fallback) {
  auto nameIt = track.find("name");
  if (nameIt == track.end()) {
    return fallback;
  }
  auto runsIt = nameIt->find("runs");
  if (runsIt == nameIt->end() || !runsIt->is_array() || runsIt->empty()) {
    return fallback;
  }
  return (*runsIt)[0].value("text", fallback);
}

TranscriptList buildTranscriptList(const std::string& videoId, const nlohmann::json& captionsJson) {
  std::unordered_map<std::string, Transcript> manuallyCreated;
  std::unordered_map<std::string, Transcript> generated;

  for (const auto& track : captionsJson.at("captionTracks")) {
    std::string languageCode = track.value("languageCode", "");
    std::string language = extractTrackDisplayName(track, languageCode);
    std::string baseUrl = stripFmtSrv3(track.value("baseUrl", ""));
    bool isGenerated = track.value("kind", "") == "asr";

    Transcript transcript(videoId, baseUrl, language, languageCode, isGenerated);
    if (isGenerated) {
      generated.emplace(languageCode, std::move(transcript));
    } else {
      manuallyCreated.emplace(languageCode, std::move(transcript));
    }
  }

  return TranscriptList(videoId, std::move(manuallyCreated), std::move(generated));
}

}  // namespace detail

Transcript::Transcript(std::string videoId, std::string url, std::string language,
                        std::string languageCode, bool isGenerated)
    : _videoId(std::move(videoId)),
      _url(std::move(url)),
      _language(std::move(language)),
      _languageCode(std::move(languageCode)),
      _isGenerated(isGenerated) {}

TranscriptList::TranscriptList(std::string videoId,
                                std::unordered_map<std::string, Transcript> manuallyCreated,
                                std::unordered_map<std::string, Transcript> generated)
    : _videoId(std::move(videoId)),
      _manuallyCreated(std::move(manuallyCreated)),
      _generated(std::move(generated)) {
  _all.reserve(_manuallyCreated.size() + _generated.size());
  for (const auto& [code, transcript] : _manuallyCreated) {
    _all.push_back(transcript);
  }
  for (const auto& [code, transcript] : _generated) {
    _all.push_back(transcript);
  }
}

Transcript TranscriptList::findTranscript(const std::vector<std::string>& languageCodes) const {
  for (const auto& code : languageCodes) {
    auto manualIt = _manuallyCreated.find(code);
    if (manualIt != _manuallyCreated.end()) {
      return manualIt->second;
    }
    auto generatedIt = _generated.find(code);
    if (generatedIt != _generated.end()) {
      return generatedIt->second;
    }
  }
  throw NoTranscriptFound(_videoId);
}

std::vector<std::string> TranscriptList::availableLanguageCodes() const {
  std::vector<std::string> codes;
  codes.reserve(_all.size());
  for (const auto& transcript : _all) {
    codes.push_back(transcript.languageCode());
  }
  return codes;
}

TranscriptList YouTubeTranscriptApi::list(const std::string& videoId) const {
  https_client::HttpClient client;

  std::string html = fetchVideoHtml(client, videoId);
  std::string apiKey = detail::extractInnertubeApiKey(html, videoId);
  nlohmann::json innertubeData = fetchInnertubeData(client, videoId, apiKey);

  detail::assertPlayability(innertubeData, videoId);
  nlohmann::json captionsJson = detail::extractCaptionsJson(innertubeData, videoId);

  return detail::buildTranscriptList(videoId, captionsJson);
}

}  // namespace youtube_utils
