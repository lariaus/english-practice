#pragma once

#include <filesystem>

namespace httplib {
class Server;
}

namespace native_server {

// Registers GET /dictionary?word=<word>&lang=<languageCode, default "en">
// and POST /dictionary/us-audio-words?lang=<languageCode, default "en">
// (JSON body {"words": [...]}, response {"words": [...]} - the cache-only
// bulk vocabulary filter ShadowLoopMode uses, see
// docs/shadow-loop-mode-spec.md). Factored out of server.cpp since it pulls
// in dictionary_utils, unlike everything else there (same reasoning as
// subtitles_route.h/storage_route.h).
//
// `serverDataDir` is the actual <dataDir>/server_data filesystem path (see
// server.cpp's registerRoutes()) - DictionaryEntry::fetch() caches
// pronunciation audio under it via ServerDataStore, and that same directory
// is separately mounted read-only at /server_data so the browser can fetch
// the cached audio back. Passed by value and captured by value in the
// route's closure (not by reference from a caller-local variable), since it
// must outlive every individual request, not just the registerRoutes()
// call that sets it up.
void registerDictionaryRoute(httplib::Server& svr, std::filesystem::path serverDataDir);

}  // namespace native_server
