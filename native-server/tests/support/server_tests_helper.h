#pragma once

#include "native_server/server.h"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace native_server_test {

constexpr const char* kTestHost = "127.0.0.1";

inline std::filesystem::path fixturesDir() {
  return std::filesystem::path(NATIVE_SERVER_TEST_FIXTURES_DIR);
}

inline std::unique_ptr<native_server::Server> startTestServer(
    uint16_t port, const std::filesystem::path& root = fixturesDir()) {
  native_server::ServerOptions options;
  options.host = kTestHost;
  options.port = port;
  options.rootDir = root;
  auto server = std::make_unique<native_server::Server>(options);
  server->start();
  return server;
}

}  // namespace native_server_test
