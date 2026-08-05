#include "youtube_video_id.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace native_server::detail {

namespace {

const std::regex& videoIdRegex() {
  static const std::regex regex("^[a-zA-Z0-9_-]{11}$");
  return regex;
}

const std::regex& embedOrShortsRegex() {
  static const std::regex regex(R"(/(?:embed|shorts)/([a-zA-Z0-9_-]{11}))");
  return regex;
}

std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\r\n\f\v");
  if (start == std::string::npos) {
    return "";
  }
  auto end = s.find_last_not_of(" \t\r\n\f\v");
  return s.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                  [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Returns the value of the first "key=..." pair in a query string
// (mirrors urllib.parse.parse_qs's "first occurrence wins" semantics for
// this use case - real YouTube URLs never repeat "v").
std::optional<std::string> extractQueryParam(const std::string& query, const std::string& key) {
  std::size_t pos = 0;
  while (pos <= query.size()) {
    std::size_t amp = query.find('&', pos);
    std::string pair = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);

    std::size_t eq = pair.find('=');
    if (eq != std::string::npos && pair.substr(0, eq) == key) {
      return pair.substr(eq + 1);
    }

    if (amp == std::string::npos) {
      break;
    }
    pos = amp + 1;
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::string> parseYoutubeVideoId(const std::string& raw) {
  std::string trimmed = trim(raw);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  if (std::regex_match(trimmed, videoIdRegex())) {
    return trimmed;
  }

  // Requires a real "scheme://" prefix - matches Python's
  // urlparse()-based check (`not parsed.scheme or not parsed.netloc`),
  // which also rejects scheme-less and protocol-relative ("//host/path")
  // inputs since either scheme or netloc ends up empty for those too.
  std::size_t schemeEnd = trimmed.find("://");
  if (schemeEnd == std::string::npos) {
    return std::nullopt;
  }

  std::string rest = trimmed.substr(schemeEnd + 3);
  std::size_t pathStart = rest.find_first_of("/?#");
  std::string netloc = pathStart == std::string::npos ? rest : rest.substr(0, pathStart);
  std::string pathAndQuery = pathStart == std::string::npos ? "" : rest.substr(pathStart);
  if (netloc.empty()) {
    return std::nullopt;
  }

  std::string hostname = netloc;
  auto atPos = hostname.find('@');
  if (atPos != std::string::npos) {
    hostname = hostname.substr(atPos + 1);
  }
  auto colonPos = hostname.find(':');
  if (colonPos != std::string::npos) {
    hostname = hostname.substr(0, colonPos);
  }
  hostname = toLower(hostname);

  std::size_t queryStart = pathAndQuery.find('?');
  std::string path =
      queryStart == std::string::npos ? pathAndQuery : pathAndQuery.substr(0, queryStart);
  std::string query = queryStart == std::string::npos ? "" : pathAndQuery.substr(queryStart + 1);

  if (hostname == "youtu.be") {
    std::string candidate = path;
    while (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    if (std::regex_match(candidate, videoIdRegex())) {
      return candidate;
    }
    return std::nullopt;
  }

  if (hostname.find("youtube.com") != std::string::npos) {
    auto vParam = extractQueryParam(query, "v");
    if (vParam && std::regex_match(*vParam, videoIdRegex())) {
      return *vParam;
    }

    std::smatch match;
    if (std::regex_search(path, match, embedOrShortsRegex())) {
      return match[1].str();
    }
  }

  return std::nullopt;
}

}  // namespace native_server::detail
