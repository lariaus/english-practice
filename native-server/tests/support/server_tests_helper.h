#pragma once

#include "native_server/server.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>

namespace native_server_test {

constexpr const char* kTestHost = "127.0.0.1";

inline std::filesystem::path fixturesDir() {
  return std::filesystem::path(NATIVE_SERVER_TEST_FIXTURES_DIR);
}

// A fresh temp directory per call - for tests exercising /storage/...,
// which need a real (if throwaway) dataDir rather than the empty default.
inline std::filesystem::path makeTempDir() {
  auto base = std::filesystem::temp_directory_path();
  std::random_device rd;
  auto dir = base / ("native_server_test_" + std::to_string(rd()));
  std::filesystem::create_directories(dir);
  return dir;
}

// dataDir defaults to empty - fine for tests that never hit /storage/...
// (the registry is simply never written to).
inline std::unique_ptr<native_server::Server> startTestServer(
    uint16_t port, const std::filesystem::path& root = fixturesDir(),
    const std::filesystem::path& dataDir = {}) {
  native_server::ServerOptions options;
  options.host = kTestHost;
  options.port = port;
  options.rootDir = root;
  options.dataDir = dataDir;
  auto server = std::make_unique<native_server::Server>(options);
  server->start();
  return server;
}

}  // namespace native_server_test
