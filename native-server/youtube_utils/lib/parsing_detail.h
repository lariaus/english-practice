#pragma once

// Internal parsing steps, factored out of youtube_transcript_api.cpp /
// transcript.cpp so they're unit-testable in isolation (fed literal
// JSON/XML fixtures) without needing network access. Not part of the
// public API - youtube_utils/youtube_transcript_api.h is that.

#include "youtube_utils/youtube_transcript_api.h"

#include <nlohmann/json.hpp>

#include <string>

namespace youtube_utils::detail {

// Throws YouTubeRequestFailed if not found.
std::string extractInnertubeApiKey(const std::string& html, const std::string& videoId);

// Throws VideoUnavailable or YouTubeRequestFailed on a non-OK status.
// Returns normally (no-op) if playabilityStatus is absent or "OK".
void assertPlayability(const nlohmann::json& innertubeData, const std::string& videoId);

// Throws TranscriptsDisabled if captions.playerCaptionsTracklistRenderer.captionTracks
// is missing.
nlohmann::json extractCaptionsJson(const nlohmann::json& innertubeData, const std::string& videoId);

std::string stripFmtSrv3(std::string url);

std::string extractTrackDisplayName(const nlohmann::json& track, const std::string& fallback);

// Builds the manual/generated TranscriptList from a captionTracks JSON
// array (the `captions.playerCaptionsTracklistRenderer` object's contents).
TranscriptList buildTranscriptList(const std::string& videoId, const nlohmann::json& captionsJson);

// Parses a caption track's raw XML body into snippets - the pure part of
// Transcript::fetch(), factored out so it's testable without a live
// network fetch.
std::vector<TranscriptSnippet> parseTranscriptXml(const std::string& xml);

}  // namespace youtube_utils::detail
