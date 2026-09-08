#include "data_packs_route.h"

#include <data_packs/data_packs.h>
#include <https_client/http_client.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <mutex>
#include <stdexcept>
#include <thread>

namespace native_server {

namespace {

void sendJson(httplib::Response& res, int status, const nlohmann::json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

// Module-level, not per-Server-instance: only one sync-or-clear operation
// is ever meaningful at a time for this whole process (see
// docs/data-pack-sync.md's "personal, single-user scope" reasoning), and
// keeping it here (rather than in Server::Impl) avoids threading it through
// registerDataPacksRoute's signature just for this. `gBusy` gates both
// POST endpoints against each other; `gProgress` is the last-known state of
// the most recent sync, read back by GET /data-packs/sync-status.
std::mutex gMutex;
bool gBusy = false;
data_packs::SyncProgress gProgress;

nlohmann::json progressToJson(const data_packs::SyncProgress& progress) {
  return {{"pack", progress.pack},
          {"total", progress.total},
          {"copied", progress.copied},
          {"done", progress.done},
          {"error", progress.error ? nlohmann::json(*progress.error) : nlohmann::json(nullptr)}};
}

std::vector<std::string> jsonToStringVector(const nlohmann::json& arr) {
  std::vector<std::string> result;
  for (const auto& item : arr) result.push_back(item.get<std::string>());
  return result;
}

// GETs another native-server instance's own (non-proxying) /data-packs -
// used both by the pulling-mode GET /data-packs?remote=... proxy below, and
// nowhere else (a sync's own background thread asks
// /data-packs/:name/files instead, a narrower, pack-specific question).
// Throws std::runtime_error on any failure (network unreachable, non-200,
// malformed body).
std::vector<std::string> fetchRemotePackList(https_client::HttpClient& client,
                                              const std::string& remote) {
  https_client::HttpResponse res;
  try {
    res = client.get("http://" + remote + "/data-packs", {});
  } catch (const https_client::HttpError& e) {
    throw std::runtime_error("Could not reach " + remote + ": " + e.what());
  }
  if (res.statusCode != 200) {
    throw std::runtime_error("Unexpected status from " + remote + ": " +
                              std::to_string(res.statusCode));
  }
  return jsonToStringVector(nlohmann::json::parse(res.body).at("packs"));
}

}  // namespace

void registerDataPacksRoute(httplib::Server& svr, std::filesystem::path sharedDataDir,
                             std::filesystem::path serverDataDir) {
  svr.Get("/data-packs", [sharedDataDir](const httplib::Request& req, httplib::Response& res) {
    if (req.has_param("remote")) {
      try {
        https_client::HttpClient client;
        auto packs = fetchRemotePackList(client, req.get_param_value("remote"));
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& name : packs) arr.push_back(name);
        sendJson(res, 200, {{"packs", arr}});
      } catch (const std::exception& e) {
        sendJson(res, 502, {{"error", e.what()}});
      }
      return;
    }

    nlohmann::json packs = nlohmann::json::array();
    for (const auto& name : data_packs::listPacks(sharedDataDir)) packs.push_back(name);
    sendJson(res, 200, {{"packs", packs}});
  });

  svr.Get(R"(/data-packs/([^/]+)/files)",
          [sharedDataDir](const httplib::Request& req, httplib::Response& res) {
            std::string name = req.matches[1];
            try {
              nlohmann::json files = nlohmann::json::array();
              for (const auto& f : data_packs::listPackFiles(sharedDataDir, name)) {
                files.push_back(f);
              }
              sendJson(res, 200, {{"files", files}});
            } catch (const std::filesystem::filesystem_error&) {
              sendJson(res, 404, {{"error", "Unknown pack: " + name}});
            }
          });

  svr.Get("/data-packs/sync-status", [](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(gMutex);
    sendJson(res, 200, progressToJson(gProgress));
  });

  svr.Get("/data-packs/server-data-size",
          [serverDataDir](const httplib::Request&, httplib::Response& res) {
            sendJson(res, 200, {{"bytes", data_packs::serverDataSizeBytes(serverDataDir)}});
          });

  svr.Post(R"(/data-packs/([^/]+)/sync)", [serverDataDir](const httplib::Request& req,
                                                            httplib::Response& res) {
    std::string name = req.matches[1];

    if (!req.has_param("remote") || req.get_param_value("remote").empty()) {
      sendJson(res, 400, {{"error", "Missing required 'remote' query parameter."}});
      return;
    }
    std::string remote = req.get_param_value("remote");

    {
      std::lock_guard<std::mutex> lock(gMutex);
      if (gBusy) {
        sendJson(res, 409, {{"error", "A sync or clear is already running"}});
        return;
      }
      gBusy = true;
      gProgress = data_packs::SyncProgress{};
      gProgress.pack = name;
      gProgress.done = false;
    }

    // Captured by value (remote, serverDataDir, name are plain
    // std::string/std::filesystem::path copies) rather than by reference to
    // anything caller-owned - this thread is detached and may still be
    // running after the Server that started it is destroyed (most
    // realistically in tests, which construct/destroy many short-lived
    // Server instances). gMutex/gBusy/gProgress are process-lifetime
    // globals, safe to touch regardless. Not joined on Server::stop() -
    // acceptable since a sync can just be re-run from scratch afterward.
    //
    // Note there is no local sharedDataDir involved here at all, even when
    // `remote` happens to be this same device's own address - every sync
    // is a real HTTP round-trip against `remote`, never a local filesystem
    // copy (see docs/data-pack-sync.md's "single path, easier to maintain
    // and test" reasoning).
    std::thread([remote, serverDataDir, name]() {
      try {
        https_client::HttpClient client;
        std::string base = "http://" + remote;

        auto listFiles = [&client, base, name]() -> std::vector<std::string> {
          https_client::HttpResponse res;
          try {
            res = client.get(base + "/data-packs/" + name + "/files", {});
          } catch (const https_client::HttpError& e) {
            throw std::runtime_error("Could not reach " + base + ": " + e.what());
          }
          if (res.statusCode == 404) {
            throw std::runtime_error("Unknown pack at " + base + ": " + name);
          }
          if (res.statusCode != 200) {
            throw std::runtime_error("Unexpected status listing files from " + base + ": " +
                                      std::to_string(res.statusCode));
          }
          return jsonToStringVector(nlohmann::json::parse(res.body).at("files"));
        };

        auto fetchBytes = [&client, base, name](const std::string& relative) -> std::string {
          https_client::HttpResponse res;
          try {
            res = client.get(base + "/shared_data/" + name + "/" + relative, {});
          } catch (const https_client::HttpError& e) {
            throw std::runtime_error("Could not fetch " + relative + " from " + base + ": " +
                                      e.what());
          }
          if (res.statusCode != 200) {
            throw std::runtime_error("Unexpected status fetching " + relative + " from " + base +
                                      ": " + std::to_string(res.statusCode));
          }
          return res.body;
        };

        data_packs::syncPack(listFiles, fetchBytes, serverDataDir,
                              [](std::size_t copiedSoFar, std::size_t total) {
                                std::lock_guard<std::mutex> lock(gMutex);
                                gProgress.copied = copiedSoFar;
                                gProgress.total = total;
                              });
      } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(gMutex);
        gProgress.error = e.what();
      }
      std::lock_guard<std::mutex> lock(gMutex);
      gProgress.done = true;
      gBusy = false;
    }).detach();

    sendJson(res, 200, {{"started", true}});
  });

  svr.Post("/data-packs/clear", [serverDataDir](const httplib::Request&, httplib::Response& res) {
    {
      std::lock_guard<std::mutex> lock(gMutex);
      if (gBusy) {
        sendJson(res, 409, {{"error", "A sync is already running"}});
        return;
      }
      gBusy = true;
    }

    data_packs::clearServerData(serverDataDir);

    {
      std::lock_guard<std::mutex> lock(gMutex);
      gBusy = false;
    }
    sendJson(res, 200, {{"status", "ok"}});
  });
}

}  // namespace native_server
