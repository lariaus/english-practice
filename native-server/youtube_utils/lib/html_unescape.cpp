#include "html_unescape.h"

#include "html_entities_generated.h"

#include <cctype>
#include <cstdlib>

namespace youtube_utils::detail {

namespace {

constexpr std::size_t kMaxEntityNameLength = 32;

std::string encodeUtf8(unsigned int codepoint) {
  std::string out;
  if (codepoint <= 0x7F) {
    out += static_cast<char>(codepoint);
  } else if (codepoint <= 0x7FF) {
    out += static_cast<char>(0xC0 | (codepoint >> 6));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    out += static_cast<char>(0xE0 | (codepoint >> 12));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (codepoint >> 18));
    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  }
  return out;
}

// Tries to consume a numeric character reference starting at text[i] == '&'.
// On success, appends the decoded character to out and returns the index
// just past the reference. On failure (malformed/unterminated), returns 0
// (the caller falls back to treating '&' as a literal character).
std::size_t tryConsumeNumericRef(const std::string& text, std::size_t i, std::string& out) {
  std::size_t pos = i + 2;  // past "&#"
  bool isHex = pos < text.size() && (text[pos] == 'x' || text[pos] == 'X');
  if (isHex) {
    ++pos;
  }

  std::size_t digitsStart = pos;
  while (pos < text.size() &&
         (isHex ? static_cast<bool>(std::isxdigit(static_cast<unsigned char>(text[pos])))
                : static_cast<bool>(std::isdigit(static_cast<unsigned char>(text[pos]))))) {
    ++pos;
  }

  if (pos == digitsStart || pos >= text.size() || text[pos] != ';') {
    return 0;
  }

  unsigned int codepoint =
      static_cast<unsigned int>(std::strtoul(text.substr(digitsStart, pos - digitsStart).c_str(),
                                              nullptr, isHex ? 16 : 10));
  out += encodeUtf8(codepoint);
  return pos + 1;  // past ';'
}

// Same contract as tryConsumeNumericRef, for named entities.
std::size_t tryConsumeNamedRef(const std::string& text, std::size_t i, std::string& out) {
  std::size_t pos = i + 1;  // past '&'
  std::size_t nameStart = pos;
  while (pos < text.size() && pos - nameStart < kMaxEntityNameLength &&
         std::isalnum(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }

  if (pos >= text.size() || text[pos] != ';' || pos == nameStart) {
    return 0;
  }

  std::string candidate = text.substr(i, pos - i + 1);  // includes '&' and ';'
  const auto& table = namedHtmlEntities();
  auto it = table.find(candidate);
  if (it == table.end()) {
    return 0;
  }

  out += it->second;
  return pos + 1;  // past ';'
}

}  // namespace

std::string htmlUnescape(const std::string& text) {
  std::string result;
  result.reserve(text.size());

  std::size_t i = 0;
  while (i < text.size()) {
    if (text[i] != '&') {
      result += text[i];
      ++i;
      continue;
    }

    bool isNumeric = i + 1 < text.size() && text[i + 1] == '#';
    std::size_t next =
        isNumeric ? tryConsumeNumericRef(text, i, result) : tryConsumeNamedRef(text, i, result);

    if (next == 0) {
      result += '&';
      ++i;
    } else {
      i = next;
    }
  }

  return result;
}

}  // namespace youtube_utils::detail
