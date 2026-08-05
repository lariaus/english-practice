#pragma once

#include <string>
#include <unordered_map>

namespace youtube_utils::detail {

const std::unordered_map<std::string, std::string>& namedHtmlEntities();

}  // namespace youtube_utils::detail
