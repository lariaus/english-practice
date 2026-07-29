#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using native_server_test::fixturesDir;
using native_server_test::kTestHost;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18080;
}

TEST_CASE("serves the index file at /", "[integration][static]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(res->body.find("fixture index") != std::string::npos);
}

TEST_CASE("serves a nested asset", "[integration][static]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/nested/asset.txt");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(res->body == "nested asset content\n");
}

TEST_CASE("serves manifest.webmanifest with the correct content type", "[integration][static]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/manifest.webmanifest");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(res->get_header_value("Content-Type") == "application/manifest+json");
}

TEST_CASE("missing file returns a JSON 404", "[integration][static]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/does-not-exist");
  REQUIRE(res);
  REQUIRE(res->status == 404);
  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("error") == "Not found");
}

TEST_CASE("server can stop and start again", "[integration][lifecycle]") {
  auto server = startTestServer(kTestPort);
  {
    httplib::Client client(kTestHost, kTestPort);
    auto res = client.Get("/");
    REQUIRE(res);
    REQUIRE(res->status == 200);
  }

  server->stop();
  REQUIRE_FALSE(server->isRunning());

  {
    httplib::Client client(kTestHost, kTestPort);
    client.set_connection_timeout(0, 200000);
    auto res = client.Get("/");
    REQUIRE_FALSE(res);
  }

  server->start();
  {
    httplib::Client client(kTestHost, kTestPort);
    auto res = client.Get("/");
    REQUIRE(res);
    REQUIRE(res->status == 200);
  }
}

TEST_CASE("rejects a literal .. traversal attempt", "[integration][security]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/../CMakeLists.txt");
  REQUIRE(res);
  REQUIRE(res->status != 200);
}

TEST_CASE("rejects a percent-encoded traversal attempt", "[integration][security]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/%2e%2e/CMakeLists.txt");
  REQUIRE(res);
  REQUIRE(res->status != 200);
}

TEST_CASE("rejects a symlink that escapes the mount root", "[integration][security]") {
  auto secretDir = std::filesystem::temp_directory_path() / "native_server_test_secret";
  std::filesystem::create_directories(secretDir);
  auto secretFile = secretDir / "secret.txt";
  {
    std::ofstream out(secretFile);
    out << "top secret";
  }

  auto linkPath = fixturesDir() / "escape_link.txt";
  std::filesystem::remove(linkPath);
  std::filesystem::create_symlink(secretFile, linkPath);

  struct SymlinkCleanup {
    std::filesystem::path path;
    ~SymlinkCleanup() { std::filesystem::remove(path); }
  } symlinkCleanup{linkPath};

  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/escape_link.txt");
  REQUIRE(res);
  REQUIRE(res->status != 200);
}

TEST_CASE("non-GET/HEAD methods on a static path are not silently allowed",
          "[integration][protocol]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Post("/index.html", "", "text/plain");
  REQUIRE(res);
  REQUIRE(res->status != 200);
}

TEST_CASE("HEAD requests are served like GET but without a body", "[integration][protocol]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Head("/index.html");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(res->body.empty());
}

TEST_CASE("GET responses include cpp-httplib's default caching headers",
          "[integration][protocol]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/index.html");
  REQUIRE(res);
  REQUIRE(res->has_header("ETag"));
  REQUIRE(res->has_header("Last-Modified"));
}
