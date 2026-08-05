#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>

using native_server_test::kTestHost;
using native_server_test::makeTempDir;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18084;
}

TEST_CASE("PUT then GET round-trips a value", "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto putRes = client.Put("/storage/maps/accounts/foo", R"({"value": [1, 2, 3]})",
                            "application/json");
  REQUIRE(putRes);
  REQUIRE(putRes->status == 200);

  auto getRes = client.Get("/storage/maps/accounts/foo");
  REQUIRE(getRes);
  REQUIRE(getRes->status == 200);
  auto body = nlohmann::json::parse(getRes->body);
  REQUIRE(body.at("value") == nlohmann::json::array({1, 2, 3}));

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET on a missing key returns 404", "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/storage/maps/accounts/missing");
  REQUIRE(res);
  REQUIRE(res->status == 404);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("PUT on a new mapId creates it transparently", "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Put("/storage/maps/brand-new-map/key", R"({"value": "hello"})",
                         "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(std::filesystem::exists(dataDir / "storage_map" / "brand-new-map.json"));

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("DELETE is idempotent", "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res1 = client.Delete("/storage/maps/accounts/never-existed");
  REQUIRE(res1);
  REQUIRE(res1->status == 200);

  auto putRes = client.Put("/storage/maps/accounts/foo", R"({"value": "bar"})", "application/json");
  REQUIRE(putRes->status == 200);

  auto delRes1 = client.Delete("/storage/maps/accounts/foo");
  REQUIRE(delRes1->status == 200);
  auto delRes2 = client.Delete("/storage/maps/accounts/foo");
  REQUIRE(delRes2->status == 200);

  auto getRes = client.Get("/storage/maps/accounts/foo");
  REQUIRE(getRes->status == 404);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("a value set via one request is visible from a separate request",
          "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);

  {
    httplib::Client writer(kTestHost, kTestPort);
    auto res = writer.Put("/storage/maps/accounts/foo", R"({"value": 42})", "application/json");
    REQUIRE(res->status == 200);
  }

  {
    httplib::Client reader(kTestHost, kTestPort);
    auto res = reader.Get("/storage/maps/accounts/foo");
    REQUIRE(res->status == 200);
    auto body = nlohmann::json::parse(res->body);
    REQUIRE(body.at("value") == 42);
  }

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("PUT with a malformed JSON body returns 400", "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Put("/storage/maps/accounts/foo", "not json", "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 400);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("PUT without a 'value' field returns 400", "[integration][storage]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Put("/storage/maps/accounts/foo", R"({"oops": 1})", "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 400);

  std::filesystem::remove_all(dataDir);
}
