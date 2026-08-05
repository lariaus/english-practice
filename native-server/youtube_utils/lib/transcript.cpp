#include "youtube_utils/youtube_transcript_api.h"

#include "html_unescape.h"
#include "parsing_detail.h"

#include <https_client/http_client.h>
#include <pugixml.hpp>

#include <regex>

namespace youtube_utils {

namespace {

std::string stripHtmlTags(const std::string& text) {
  static const std::regex kTagRegex("<[^>]*>");
  return std::regex_replace(text, kTagRegex, "");
}

}  // namespace

namespace detail {

// Caption XML text nodes are double-HTML-escaped in practice (this is why
// youtube_transcript_api itself calls html.unescape() again on top of its
// XML parser's own entity decoding) - pugixml decodes the outer, standard-
// XML layer during parsing here; the explicit htmlUnescape() call below
// handles the second layer, mirroring Python exactly.
std::vector<TranscriptSnippet> parseTranscriptXml(const std::string& xml) {
  pugi::xml_document doc;
  pugi::xml_parse_result parseResult = doc.load_buffer(xml.data(), xml.size());
  if (!parseResult) {
    throw YouTubeRequestFailed(std::string("Could not parse caption XML: ") +
                                parseResult.description());
  }

  std::vector<TranscriptSnippet> snippets;
  for (pugi::xml_node textNode : doc.document_element().children("text")) {
    std::string rawText = textNode.text().get();
    if (rawText.empty()) {
      continue;
    }

    TranscriptSnippet snippet;
    snippet.text = stripHtmlTags(htmlUnescape(rawText));
    snippet.start = textNode.attribute("start").as_double();
    snippet.duration = textNode.attribute("dur").as_double(0.0);
    snippets.push_back(std::move(snippet));
  }

  return snippets;
}

}  // namespace detail

FetchedTranscript Transcript::fetch() const {
  https_client::HttpClient client;
  https_client::HttpResponse res;
  try {
    res = client.get(_url, {});
  } catch (const https_client::HttpError& e) {
    throw YouTubeRequestFailed(std::string("Could not fetch captions: ") + e.what());
  }

  if (res.statusCode != 200) {
    throw YouTubeRequestFailed("Unexpected status fetching captions: " +
                                std::to_string(res.statusCode));
  }

  FetchedTranscript result;
  result.videoId = _videoId;
  result.language = _language;
  result.languageCode = _languageCode;
  result.isGenerated = _isGenerated;
  result.snippets = detail::parseTranscriptXml(res.body);

  return result;
}

}  // namespace youtube_utils
