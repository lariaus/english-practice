#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace youtube_utils {

struct TranscriptSnippet {
  std::string text;
  double start = 0.0;
  double duration = 0.0;
};

struct FetchedTranscript {
  std::vector<TranscriptSnippet> snippets;
  std::string videoId;
  std::string language;
  std::string languageCode;
  bool isGenerated = false;
};

class YouTubeTranscriptApiError : public std::runtime_error {
 public:
  explicit YouTubeTranscriptApiError(const std::string& message) : std::runtime_error(message) {}
};

class VideoUnavailable : public YouTubeTranscriptApiError {
 public:
  explicit VideoUnavailable(const std::string& videoId)
      : YouTubeTranscriptApiError("Video unavailable or does not exist: " + videoId) {}
};

class TranscriptsDisabled : public YouTubeTranscriptApiError {
 public:
  explicit TranscriptsDisabled(const std::string& videoId)
      : YouTubeTranscriptApiError("Captions are disabled for this video: " + videoId) {}
};

class NoTranscriptFound : public YouTubeTranscriptApiError {
 public:
  explicit NoTranscriptFound(const std::string& videoId)
      : YouTubeTranscriptApiError(
            "No transcript found for the requested language(s) in video: " + videoId) {}
};

// Mirrors server.py's generic `except Exception` -> 502 path: anything that
// isn't specifically VideoUnavailable/TranscriptsDisabled/NoTranscriptFound
// (a network failure, an unparseable response, an unrecognized
// playabilityStatus) collapses into this one.
class YouTubeRequestFailed : public YouTubeTranscriptApiError {
 public:
  explicit YouTubeRequestFailed(const std::string& message) : YouTubeTranscriptApiError(message) {}
};

class Transcript {
 public:
  Transcript(std::string videoId, std::string url, std::string language,
             std::string languageCode, bool isGenerated);

  const std::string& language() const { return _language; }
  const std::string& languageCode() const { return _languageCode; }
  bool isGenerated() const { return _isGenerated; }

  // Throws YouTubeRequestFailed on network/parse failure.
  FetchedTranscript fetch() const;

 private:
  std::string _videoId;
  std::string _url;
  std::string _language;
  std::string _languageCode;
  bool _isGenerated;
};

class TranscriptList {
 public:
  TranscriptList(std::string videoId, std::unordered_map<std::string, Transcript> manuallyCreated,
                 std::unordered_map<std::string, Transcript> generated);

  // Manually-created transcripts are preferred over generated ones for a
  // given language code; walks languageCodes in the given priority order.
  // Throws NoTranscriptFound if none of the requested codes match either.
  Transcript findTranscript(const std::vector<std::string>& languageCodes) const;

  std::vector<std::string> availableLanguageCodes() const;

  std::vector<Transcript>::const_iterator begin() const { return _all.begin(); }
  std::vector<Transcript>::const_iterator end() const { return _all.end(); }

 private:
  std::string _videoId;
  std::unordered_map<std::string, Transcript> _manuallyCreated;
  std::unordered_map<std::string, Transcript> _generated;
  std::vector<Transcript> _all;
};

class YouTubeTranscriptApi {
 public:
  // Throws VideoUnavailable, TranscriptsDisabled, or YouTubeRequestFailed.
  TranscriptList list(const std::string& videoId) const;
};

}  // namespace youtube_utils
