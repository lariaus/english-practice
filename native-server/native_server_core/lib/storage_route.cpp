#include "storage_route.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <storage_map/storage_map.h>

#include <string>

namespace native_server {

namespace {

void sendJson(httplib::Response& res, int status, const nlohmann::json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

}  // namespace

void registerStorageRoutes(httplib::Server& svr, storage_map::StorageMapRegistry& registry) {
  static const char* kPathPattern = R"(/storage/maps/([^/]+)/([^/]+))";

  svr.Get(kPathPattern, [&registry](const httplib::Request& req, httplib::Response& res) {
    const std::string& mapId = req.matches[1];
    const std::string& key = req.matches[2];

    auto value = registry.getOrCreate(mapId).get(key);
    if (!value) {
      sendJson(res, 404, {{"error", "Key not found"}});
      return;
    }
    sendJson(res, 200, {{"value", *value}});
  });

  svr.Put(kPathPattern, [&registry](const httplib::Request& req, httplib::Response& res) {
    const std::string& mapId = req.matches[1];
    const std::string& key = req.matches[2];

    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
      sendJson(res, 400, {{"error", std::string("Invalid JSON body: ") + e.what()}});
      return;
    }
    if (!body.contains("value")) {
      sendJson(res, 400, {{"error", "Missing 'value' field in request body"}});
      return;
    }

    registry.getOrCreate(mapId).set(key, body.at("value"));
    sendJson(res, 200, {{"status", "ok"}});
  });

  svr.Delete(kPathPattern, [&registry](const httplib::Request& req, httplib::Response& res) {
    const std::string& mapId = req.matches[1];
    const std::string& key = req.matches[2];

    registry.getOrCreate(mapId).remove(key);
    sendJson(res, 200, {{"status", "ok"}});
  });
}

}  // namespace native_server
