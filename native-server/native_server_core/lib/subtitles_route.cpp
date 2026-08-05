#include "subtitles_route.h"

#include "youtube_video_id.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <youtube_utils/youtube_transcript_api.h>

#include <optional>
#include <set>
#include <string>

namespace native_server {

namespace {

void sendJson(httplib::Response& res, int status, const nlohmann::json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

}  // namespace

void registerSubtitlesRoute(httplib::Server& svr) {
  svr.Get("/subtitles", [](const httplib::Request& req, httplib::Response& res) {
    std::string rawUrl = req.get_param_value("url");
    std::string language = req.has_param("lang") ? req.get_param_value("lang") : "en";

    auto videoId = detail::parseYoutubeVideoId(rawUrl);
    if (!videoId) {
      sendJson(res, 400,
               {{"error", "Missing or unparseable 'url' query parameter (expected a YouTube "
                          "URL or video ID)."}});
      return;
    }

    youtube_utils::YouTubeTranscriptApi api;
    std::optional<youtube_utils::TranscriptList> transcriptList;
    try {
      transcriptList = api.list(*videoId);
    } catch (const youtube_utils::VideoUnavailable&) {
      sendJson(res, 404, {{"error", "Video unavailable or does not exist."}});
      return;
    } catch (const youtube_utils::TranscriptsDisabled&) {
      sendJson(res, 404, {{"error", "Captions are disabled for this video."}});
      return;
    } catch (const std::exception& e) {
      sendJson(res, 502, {{"error", std::string("Could not look up captions: ") + e.what()}});
      return;
    }

    std::optional<youtube_utils::Transcript> transcript;
    try {
      transcript = transcriptList->findTranscript({language});
    } catch (const youtube_utils::NoTranscriptFound&) {
      // Handled below - transcript stays unset.
    }

    if (!transcript) {
      std::vector<std::string> availableCodes = transcriptList->availableLanguageCodes();
      std::set<std::string> availableSet(availableCodes.begin(), availableCodes.end());
      sendJson(res, 404,
               {{"error", "No '" + language +
                              "' captions (manual or auto-generated) found for this video."},
                {"availableLanguages", std::vector<std::string>(availableSet.begin(), availableSet.end())}});
      return;
    }

    youtube_utils::FetchedTranscript fetched;
    try {
      fetched = transcript->fetch();
    } catch (const std::exception& e) {
      sendJson(res, 502, {{"error", std::string("Could not fetch captions: ") + e.what()}});
      return;
    }

    nlohmann::json cues = nlohmann::json::array();
    for (const auto& snippet : fetched.snippets) {
      cues.push_back({{"text", snippet.text}, {"start", snippet.start}, {"duration", snippet.duration}});
    }

    sendJson(res, 200,
             {{"videoId", *videoId},
              {"language", transcript->language()},
              {"languageCode", transcript->languageCode()},
              {"isGenerated", transcript->isGenerated()},
              {"cues", cues}});
  });
}

}  // namespace native_server
