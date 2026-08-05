#pragma once

#include <string>

namespace youtube_utils::detail {

// Mirrors Python's html.unescape() for the subset that matters here: named
// entities requiring a trailing semicolon (see html_entities_generated.cpp
// for the full HTML5 table) and numeric/hex character references
// (&#NNNN; / &#xHHHH;). Deliberately does not implement the ~106 legacy
// named entities that don't require a trailing semicolon (e.g. bare
// "&amp") - real YouTube caption XML always emits well-formed entities
// with a semicolon, so this isn't a practical gap. Also does not implement
// the HTML5 spec's Windows-1252 remapping for out-of-range numeric refs in
// 0x80-0x9F (an obscure legacy-encoding corner case, not expected in real
// captions).
std::string htmlUnescape(const std::string& text);

}  // namespace youtube_utils::detail
