#include "native_server/capi.h"

#include "native_server/server.h"

namespace {

native_server::Server* toServer(NativeServerHandle handle) {
  return static_cast<native_server::Server*>(handle);
}

}  // namespace

NativeServerHandle native_server_create(const char* host, int port, const char* rootDir,
                                         const char* dataDir) {
  try {
    native_server::ServerOptions options;
    options.host = host;
    options.port = static_cast<uint16_t>(port);
    options.rootDir = rootDir;
    options.dataDir = dataDir;
    return new native_server::Server(std::move(options));
  } catch (...) {
    return nullptr;
  }
}

int native_server_start(NativeServerHandle handle) {
  if (!handle) return 1;
  try {
    toServer(handle)->start();
    return 0;
  } catch (...) {
    return 1;
  }
}

void native_server_stop(NativeServerHandle handle) {
  if (!handle) return;
  try {
    toServer(handle)->stop();
  } catch (...) {
  }
}

void native_server_destroy(NativeServerHandle handle) {
  if (!handle) return;
  try {
    delete toServer(handle);
  } catch (...) {
  }
}
