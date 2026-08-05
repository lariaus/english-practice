#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace native_server_cli {

struct ResolvedConfig {
  std::string host;
  uint16_t port;
  std::string dir;
  // Unlike dir, has a default - not required to already exist (created on
  // first write), so there's nothing to fail without it.
  std::string dataDir;
};

class ConfigError : public std::runtime_error {
 public:
  explicit ConfigError(const std::string& message) : std::runtime_error(message) {}
};

using EnvLookup = std::function<const char*(const char*)>;

// args excludes argv[0]. Throws ConfigError (with a human-readable message)
// if the served directory can't be resolved from either --dir or
// NATIVE_SERVER_DIR, or if an unknown flag/invalid port is given.
// Precedence: CLI flag > env var > default (host/port/dataDir only - dir
// has no default). dataDir defaults to ".app_data" - relative to the CWD
// the CLI is invoked from (resolved to an absolute path in main.cpp),
// not home-relative, matching the documented "run from the repo root"
// usage.
ResolvedConfig resolveConfig(const std::vector<std::string>& args, const EnvLookup& envLookup);

}  // namespace native_server_cli
