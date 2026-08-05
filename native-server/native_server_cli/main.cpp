#include "native_server/server.h"
#include "native_server_cli/config.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {

std::atomic<native_server::Server*> gServer{nullptr};

void handleSignal(int) {
  if (auto* server = gServer.load()) server->stop();
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);

  native_server_cli::ResolvedConfig config;
  try {
    config = native_server_cli::resolveConfig(args,
                                               [](const char* name) { return std::getenv(name); });
  } catch (const native_server_cli::ConfigError& e) {
    std::cerr << "native-server: " << e.what() << "\n";
    std::cerr << "usage: native_server_cli --dir <path> [--host <host>] [--port <port>] "
                 "[--data-dir <path>]\n";
    return 1;
  }

  native_server::ServerOptions options;
  options.host = config.host;
  options.port = config.port;
  options.rootDir = std::filesystem::absolute(config.dir);
  options.dataDir = std::filesystem::absolute(config.dataDir);
  options.enableStdoutLogging = true;

  std::unique_ptr<native_server::Server> server;
  try {
    server = std::make_unique<native_server::Server>(options);
  } catch (const native_server::ServerError& e) {
    std::cerr << "native-server: " << e.what() << "\n";
    return 1;
  }

  gServer.store(server.get());
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  try {
    server->start();
  } catch (const native_server::ServerError& e) {
    std::cerr << "native-server: failed to start: " << e.what() << "\n";
    return 1;
  }

  std::cout << "native-server listening on http://" << options.host << ":" << options.port
            << ", serving " << options.rootDir.string() << "\n";

  while (server->isRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  return 0;
}
