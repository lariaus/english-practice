#include "native_server/server.h"

// cpp-httplib's own default (5) is smaller than kThreadPoolSize below, so a
// burst of concurrent connections gets refused/reset by the OS accept queue
// before ever reaching the worker pool - confirmed by this project's own
// concurrency-load test. Must be defined before <httplib.h> is included.
#define CPPHTTPLIB_LISTEN_BACKLOG 64

#include <httplib.h>
#include <nlohmann/json.hpp>

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
  explicit Impl(ServerOptions opts) : options(std::move(opts)) {}

  ServerOptions options;
  httplib::Server svr;
  std::thread listenThread;
  std::atomic<bool> running{false};

  void registerRoutes() {
    if (!svr.set_mount_point("/", options.rootDir.string())) {
      throw ServerError("Failed to mount rootDir: " + options.rootDir.string());
    }
    svr.set_file_extension_and_mimetype_mapping("webmanifest", "application/manifest+json");

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
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
