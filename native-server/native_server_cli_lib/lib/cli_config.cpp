#include "native_server_cli/config.h"

#include <optional>

namespace native_server_cli {

namespace {

std::optional<std::string> envOrNull(const EnvLookup& envLookup, const char* name) {
  const char* value = envLookup(name);
  if (value == nullptr || std::string(value).empty()) return std::nullopt;
  return std::string(value);
}

}  // namespace

ResolvedConfig resolveConfig(const std::vector<std::string>& args, const EnvLookup& envLookup) {
  std::optional<std::string> cliHost;
  std::optional<std::string> cliPortStr;
  std::optional<std::string> cliDir;

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= args.size()) throw ConfigError("Missing value for " + arg);
      return args[++i];
    };

    if (arg == "--host") {
      cliHost = next();
    } else if (arg == "--port") {
      cliPortStr = next();
    } else if (arg == "--dir") {
      cliDir = next();
    } else {
      throw ConfigError("Unknown argument: " + arg);
    }
  }

  ResolvedConfig config;
  config.host = cliHost.value_or(envOrNull(envLookup, "HOST").value_or("0.0.0.0"));

  std::string portStr = cliPortStr.value_or(envOrNull(envLookup, "PORT").value_or("8000"));
  try {
    unsigned long parsed = std::stoul(portStr);
    if (parsed == 0 || parsed > 65535) throw std::out_of_range("port range");
    config.port = static_cast<uint16_t>(parsed);
  } catch (const std::exception&) {
    throw ConfigError("Invalid port: " + portStr);
  }

  std::optional<std::string> dir = cliDir ? cliDir : envOrNull(envLookup, "NATIVE_SERVER_DIR");
  if (!dir) {
    throw ConfigError("Missing served directory - pass --dir <path> or set NATIVE_SERVER_DIR");
  }
  config.dir = *dir;

  return config;
}

}  // namespace native_server_cli
