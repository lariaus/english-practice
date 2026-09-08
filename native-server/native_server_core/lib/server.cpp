#include "native_server/server.h"

// cpp-httplib's own default (5) is smaller than kThreadPoolSize below, so a
// burst of concurrent connections gets refused/reset by the OS accept queue
// before ever reaching the worker pool - confirmed by this project's own
// concurrency-load test. Must be defined before <httplib.h> is included.
#define CPPHTTPLIB_LISTEN_BACKLOG 64

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "data_packs_route.h"
#include "dictionary_route.h"
#include "storage_route.h"
#include "subtitles_route.h"

#include <storage_map/storage_map.h>

#include <atomic>
#include <ctime>
#include <iostream>
#include <thread>

namespace native_server {

namespace {

constexpr std::size_t kThreadPoolSize = 8;

std::string jsonError(const std::string& message) {
  return nlohmann::json{{"error", message}}.dump();
}

// Matches Python http.server's log_date_time_string() format, e.g.
// "29/Jul/2026 14:32:01".
std::string formatLogTimestamp() {
  std::time_t now = std::time(nullptr);
  std::tm localTime{};
  localtime_r(&now, &localTime);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%d/%b/%Y %H:%M:%S", &localTime);
  return buffer;
}

}  // namespace

struct Server::Impl {
  explicit Impl(ServerOptions opts) : options(std::move(opts)), storageRegistry(options.dataDir) {}

  ServerOptions options;
  httplib::Server svr;
  storage_map::StorageMapRegistry storageRegistry;
  std::thread listenThread;
  std::atomic<bool> running{false};

  void registerRoutes() {
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
      res.set_content(nlohmann::json{{"status", "ok"}, {"service", "NativeServer"}}.dump(),
                       "application/json");
    });
    registerSubtitlesRoute(svr);
    registerStorageRoutes(svr, storageRegistry);

    // set_mount_point requires the target directory to already exist (it
    // just stat()s it and returns false otherwise - no lazy creation, unlike
    // StorageMapRegistry/ServerDataStore which both create their own
    // directories on first write) - so this must happen before mounting
    // below, or a brand-new install (no word ever looked up yet) would fail
    // to start the server at all, not just the dictionary feature.
    std::filesystem::path serverDataDir = options.dataDir / "server_data";
    std::error_code serverDataDirEc;
    std::filesystem::create_directories(serverDataDir, serverDataDirEc);
    if (serverDataDirEc) {
      throw ServerError("Failed to create server data directory: " + serverDataDir.string());
    }
    registerDictionaryRoute(svr, serverDataDir);
    // sharedDataDir is empty on every build except the CLI's own config -
    // see server.h's own comment on ServerOptions::sharedDataDir and
    // data_packs_route.h - always safe to register unconditionally.
    registerDataPacksRoute(svr, options.sharedDataDir, serverDataDir);

    if (!svr.set_mount_point("/", options.rootDir.string())) {
      throw ServerError("Failed to mount rootDir: " + options.rootDir.string());
    }
    if (!svr.set_mount_point("/server_data", serverDataDir.string())) {
      throw ServerError("Failed to mount server_data dir: " + serverDataDir.string());
    }
    // Serves a pack's raw file bytes by relative path (e.g.
    // /shared_data/my-pack/dictionaries/wiktionaryapi/en-word.json) to
    // whichever other device's data_packs_route.cpp is pulling this pack -
    // see docs/data-pack-sync.md. sharedDataDir is empty on every build
    // except the CLI's own config, so this mount (unlike rootDir/
    // server_data above, both always required) is skipped entirely rather
    // than mounting a nonsensical empty path.
    if (!options.sharedDataDir.empty()) {
      std::error_code sharedDataDirEc;
      std::filesystem::create_directories(options.sharedDataDir, sharedDataDirEc);
      if (sharedDataDirEc) {
        throw ServerError("Failed to create shared data directory: " +
                           options.sharedDataDir.string());
      }
      if (!svr.set_mount_point("/shared_data", options.sharedDataDir.string())) {
        throw ServerError("Failed to mount shared_data dir: " + options.sharedDataDir.string());
      }
    }
    svr.set_file_extension_and_mimetype_mapping("webmanifest", "application/manifest+json");

    // cpp-httplib's static-file serving (the "/" mount above) sets no
    // Cache-Control header at all, so WKWebView falls back to its own
    // heuristic HTTP caching - which persists across app reinstalls unless
    // the app is deleted first, not just re-run/redeployed over the
    // existing install. That silently pins the webapp to whatever
    // index.html/JS/CSS bundle happened to be cached from a previous
    // build, even though the server itself is serving fresh files. This is
    // a same-device, same-process server with no real caching benefit to
    // preserve, so disable HTTP caching outright on every response rather
    // than trying to cache-bust individual asset URLs.
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
      res.set_header("Cache-Control", "no-store");
    });

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
      // cpp-httplib calls this unconditionally for any status >= 400 - if
      // a route handler already set a specific JSON error body (e.g.
      // /subtitles), leave it alone; only fill in a generic message when
      // nothing was set (e.g. httplib's own built-in static-file 404).
      if (!res.body.empty()) {
        return;
      }
      std::string message = res.status == 404 ? "Not found" : "Error";
      res.set_content(jsonError(message), "application/json");
    });

    svr.new_task_queue = [] { return new httplib::ThreadPool(kThreadPoolSize); };

    if (options.enableStdoutLogging) {
      svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << req.remote_addr << " - [" << formatLogTimestamp() << "] \"" << req.method
                  << " " << req.path << " " << req.version << "\" " << res.status << "\n";
      });
    }
  }
};

Server::Server(ServerOptions options) : _impl(std::make_unique<Impl>(std::move(options))) {
  std::error_code ec;
  if (!std::filesystem::is_directory(_impl->options.rootDir, ec) || ec) {
    throw ServerError("rootDir is not a directory: " + _impl->options.rootDir.string());
  }
  _impl->registerRoutes();
}

Server::~Server() {
  stop();
}

void Server::start() {
  if (_impl->running.load()) return;

  if (!_impl->svr.bind_to_port(_impl->options.host, _impl->options.port)) {
    throw ServerError("Failed to bind " + _impl->options.host + ":" +
                       std::to_string(_impl->options.port));
  }

  _impl->running.store(true);
  _impl->listenThread = std::thread([this] { _impl->svr.listen_after_bind(); });
}

void Server::stop() {
  if (!_impl->running.load()) return;
  _impl->svr.stop();
  if (_impl->listenThread.joinable()) _impl->listenThread.join();
  _impl->running.store(false);
}

bool Server::isRunning() const {
  return _impl->running.load();
}

}  // namespace native_server
