#pragma once

#include <filesystem>

namespace httplib {
class Server;
}

namespace native_server {

// Registers GET /data-packs (list-local, or proxy-to-remote when a
// `?remote=host:port` query param is given), GET /data-packs/:name/files
// (serving-side, lists a local pack's relative file paths),
// POST /data-packs/:name/sync (always pulls from a required `?remote=`
// address, even a self-targeting one - never a local copy),
// GET /data-packs/sync-status, and POST /data-packs/clear - see
// docs/data-pack-sync.md. Factored out of server.cpp since it pulls in
// data_packs and https_client, unlike everything else there (same
// reasoning as subtitles_route.h/storage_route.h/dictionary_route.h). The
// static `/shared_data` mount a sync's remote side reads bytes from is
// registered directly in server.cpp instead, mirroring `/server_data`'s own
// mount there.
//
// `sharedDataDir` is empty on every build except the CLI's own config -
// these routes treat that as "no packs available" rather than an error, so
// they're always safe to register unconditionally (see server.h's own
// comment on ServerOptions::sharedDataDir). `serverDataDir` is the same
// <dataDir>/server_data path dictionary_route.h already documents - the
// sync destination.
//
// Both paths are passed by value and captured by value in each route's own
// closure (not by reference from a caller-local variable), since they must
// outlive every individual request, not just the registerRoutes() call
// that sets them up - same convention as registerDictionaryRoute.
void registerDataPacksRoute(httplib::Server& svr, std::filesystem::path sharedDataDir,
                             std::filesystem::path serverDataDir);

}  // namespace native_server
