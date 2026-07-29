#include "native_server_cli/config.h"

#include <catch2/catch_test_macros.hpp>

#include <map>

namespace {

native_server_cli::EnvLookup makeEnv(std::map<std::string, std::string> vars) {
  return [vars](const char* name) -> const char* {
    auto it = vars.find(name);
    return it != vars.end() ? it->second.c_str() : nullptr;
  };
}

}  // namespace

TEST_CASE("resolveConfig applies host/port defaults", "[config]") {
  auto env = makeEnv({});
  auto config = native_server_cli::resolveConfig({"--dir", "/tmp/whatever"}, env);
  REQUIRE(config.host == "0.0.0.0");
  REQUIRE(config.port == 8000);
  REQUIRE(config.dir == "/tmp/whatever");
}

TEST_CASE("resolveConfig throws when no directory is resolvable", "[config]") {
  auto env = makeEnv({});
  REQUIRE_THROWS_AS(native_server_cli::resolveConfig({}, env), native_server_cli::ConfigError);
}

TEST_CASE("resolveConfig reads host/port/dir from env vars", "[config]") {
  auto env =
      makeEnv({{"HOST", "127.0.0.1"}, {"PORT", "9000"}, {"NATIVE_SERVER_DIR", "/srv/dist"}});
  auto config = native_server_cli::resolveConfig({}, env);
  REQUIRE(config.host == "127.0.0.1");
  REQUIRE(config.port == 9000);
  REQUIRE(config.dir == "/srv/dist");
}

TEST_CASE("resolveConfig CLI flags take precedence over env vars", "[config]") {
  auto env =
      makeEnv({{"HOST", "127.0.0.1"}, {"PORT", "9000"}, {"NATIVE_SERVER_DIR", "/srv/dist"}});
  auto config = native_server_cli::resolveConfig(
      {"--host", "0.0.0.0", "--port", "8080", "--dir", "/other"}, env);
  REQUIRE(config.host == "0.0.0.0");
  REQUIRE(config.port == 8080);
  REQUIRE(config.dir == "/other");
}

TEST_CASE("resolveConfig rejects an invalid port", "[config]") {
  auto env = makeEnv({});
  REQUIRE_THROWS_AS(
      native_server_cli::resolveConfig({"--dir", "/tmp", "--port", "not-a-number"}, env),
      native_server_cli::ConfigError);
  REQUIRE_THROWS_AS(
      native_server_cli::resolveConfig({"--dir", "/tmp", "--port", "70000"}, env),
      native_server_cli::ConfigError);
}

TEST_CASE("resolveConfig rejects an unknown flag", "[config]") {
  auto env = makeEnv({});
  REQUIRE_THROWS_AS(native_server_cli::resolveConfig({"--dir", "/tmp", "--bogus"}, env),
                     native_server_cli::ConfigError);
}
