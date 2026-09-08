#pragma once

#include "dictionary_utils/free_dictionary_api_entry.h"

#include <nlohmann/json.hpp>

namespace dictionary_utils::detail {

// Pure transform from freedictionaryapi.com's JSON response shape into
// WordInfo - no network involved, so unit tests can exercise this directly
// against fixture JSON instead of hitting the real API.
WordInfo parseWordInfo(const nlohmann::json& data);

}  // namespace dictionary_utils::detail
