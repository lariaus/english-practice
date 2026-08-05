#pragma once

namespace httplib {
class Server;
}

namespace storage_map {
class StorageMapRegistry;
}

namespace native_server {

// Registers GET/PUT/DELETE /storage/maps/:mapId/:key. Factored out of
// server.cpp since it pulls in storage_map, unlike everything else there.
void registerStorageRoutes(httplib::Server& svr, storage_map::StorageMapRegistry& registry);

}  // namespace native_server
