#pragma once

// Direct C++ port of native-exp-server/server.py's parse_youtube_video_id -
// accepts a full YouTube URL (watch/shorts/youtu.be/embed) or a bare
// 11-char video ID, returns the video ID or nullopt if it can't be parsed.
// Kept private to native_server_core (not part of youtube_utils, which
// takes a plain video ID already, matching Python's own library).

#include <optional>
#include <string>

namespace native_server::detail {

std::optional<std::string> parseYoutubeVideoId(const std::string& raw);

}  // namespace native_server::detail
