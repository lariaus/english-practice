#include "parsing_detail.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parses text/start/duration from a basic transcript", "[youtube_utils][transcript_xml]") {
  std::string xml = R"(<?xml version="1.0" encoding="utf-8" ?><transcript>
<text start="1.5" dur="3.2">Hello world</text>
</transcript>)";

  auto snippets = youtube_utils::detail::parseTranscriptXml(xml);
  REQUIRE(snippets.size() == 1);
  REQUIRE(snippets[0].text == "Hello world");
  REQUIRE(snippets[0].start == 1.5);
  REQUIRE(snippets[0].duration == 3.2);
}

TEST_CASE("defaults duration to 0.0 when dur is absent", "[youtube_utils][transcript_xml]") {
  std::string xml = R"(<transcript><text start="4.0">No duration attribute</text></transcript>)";

  auto snippets = youtube_utils::detail::parseTranscriptXml(xml);
  REQUIRE(snippets.size() == 1);
  REQUIRE(snippets[0].duration == 0.0);
}

TEST_CASE("skips text nodes with no text content", "[youtube_utils][transcript_xml]") {
  std::string xml =
      R"(<transcript><text start="0" dur="1"></text><text start="1" dur="1">Real line</text></transcript>)";

  auto snippets = youtube_utils::detail::parseTranscriptXml(xml);
  REQUIRE(snippets.size() == 1);
  REQUIRE(snippets[0].text == "Real line");
}

TEST_CASE("strips HTML tags from the text", "[youtube_utils][transcript_xml]") {
  std::string xml = R"(<transcript><text start="0" dur="1">&lt;i&gt;Hello&lt;/i&gt; world</text></transcript>)";

  auto snippets = youtube_utils::detail::parseTranscriptXml(xml);
  REQUIRE(snippets.size() == 1);
  REQUIRE(snippets[0].text == "Hello world");
}

TEST_CASE("unescapes double-HTML-escaped entities in the text",
          "[youtube_utils][transcript_xml]") {
  // The raw XML text node is "It&amp;#39;s here" - a single round of XML
  // parsing turns "&amp;" into "&", leaving the literal string
  // "&#39;s here" as the extracted text - exactly the double-escaping this
  // whole path exists to handle (see transcript.cpp).
  std::string xml = R"(<transcript><text start="0" dur="1">It&amp;#39;s here</text></transcript>)";

  auto snippets = youtube_utils::detail::parseTranscriptXml(xml);
  REQUIRE(snippets.size() == 1);
  REQUIRE(snippets[0].text == "It's here");
}
