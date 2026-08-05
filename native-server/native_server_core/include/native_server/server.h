#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace native_server {

struct ServerOptions {
  std::string host = "0.0.0.0";
  uint16_t port = 8000;
  std::filesystem::path rootDir;
  // Backs /storage/maps/... - unlike rootDir, doesn't need to already
  // exist (created on first write). May be left empty if the /storage
  // routes are never actually used (e.g. existing tests unrelated to
  // storage).
  std::filesystem::path dataDir;
  bool enableStdoutLogging = false;
};

class ServerError : public std::runtime_error {
 public:
  explicit ServerError(const std::string& message) : std::runtime_error(message) {}
};

class Server {
 public:
  explicit Server(ServerOptions options);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void start();
  void stop();
  bool isRunning() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

}  // namespace native_server
