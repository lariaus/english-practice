#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using native_server_test::kTestHost;
using native_server_test::makeTempDir;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18085;
constexpr uint16_t kTestPort2 = 18086;

void writeFile(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file << contents;
}

// Polls /data-packs/sync-status until done, or gives up after a generous
// timeout - a real sync in these tests is a handful of tiny files over
// loopback HTTP, so this should resolve within a few iterations at most.
nlohmann::json waitForSyncDone(httplib::Client& client) {
  for (int i = 0; i < 500; ++i) {
    auto res = client.Get("/data-packs/sync-status");
    REQUIRE(res);
    auto body = nlohmann::json::parse(res->body);
    if (body.at("done").get<bool>()) return body;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  FAIL("sync did not finish in time");
  return {};
}

std::string remoteAddress(uint16_t port) {
  return std::string(kTestHost) + ":" + std::to_string(port);
}

}  // namespace

TEST_CASE("GET /data-packs (no remote) returns an empty list when sharedDataDir is unset",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/data-packs");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto body = nlohmann::json::parse(res->body);
  CHECK(body.at("packs") == nlohmann::json::array());

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /data-packs (no remote) lists real pack directories",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto sharedDataDir = makeTempDir();
  writeFile(sharedDataDir / "pack-a" / "file.json", "{}");
  writeFile(sharedDataDir / "pack-b" / "file.json", "{}");
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir, sharedDataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/data-packs");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto body = nlohmann::json::parse(res->body);
  CHECK(body.at("packs") == nlohmann::json::array({"pack-a", "pack-b"}));

  std::filesystem::remove_all(dataDir);
  std::filesystem::remove_all(sharedDataDir);
}

TEST_CASE("GET /data-packs?remote=... proxies to another server's own pack list",
          "[integration][data_packs]") {
  auto sourceDataDir = makeTempDir();
  auto sourceSharedDataDir = makeTempDir();
  writeFile(sourceSharedDataDir / "pack-a" / "file.json", "{}");
  auto source = startTestServer(kTestPort, native_server_test::fixturesDir(), sourceDataDir,
                                 sourceSharedDataDir);

  auto pullerDataDir = makeTempDir();
  auto puller = startTestServer(kTestPort2, native_server_test::fixturesDir(), pullerDataDir);
  httplib::Client pullerClient(kTestHost, kTestPort2);

  auto res = pullerClient.Get(("/data-packs?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto body = nlohmann::json::parse(res->body);
  CHECK(body.at("packs") == nlohmann::json::array({"pack-a"}));

  std::filesystem::remove_all(sourceDataDir);
  std::filesystem::remove_all(sourceSharedDataDir);
  std::filesystem::remove_all(pullerDataDir);
}

TEST_CASE("GET /data-packs?remote=... returns 502 when the remote is unreachable",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get(("/data-packs?remote=" + remoteAddress(kTestPort2)).c_str());
  REQUIRE(res);
  CHECK(res->status == 502);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /data-packs/:name/files lists a real pack's relative file paths",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto sharedDataDir = makeTempDir();
  writeFile(sharedDataDir / "my-pack" / "dictionaries" / "en-word.json", "{}");
  writeFile(sharedDataDir / "my-pack" / "dictionaries" / "en-us-word.ogg", "audio");
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir, sharedDataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/data-packs/my-pack/files");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto files = nlohmann::json::parse(res->body).at("files").get<std::vector<std::string>>();
  std::sort(files.begin(), files.end());
  CHECK(files == std::vector<std::string>{"dictionaries/en-us-word.ogg", "dictionaries/en-word.json"});

  std::filesystem::remove_all(dataDir);
  std::filesystem::remove_all(sharedDataDir);
}

TEST_CASE("GET /data-packs/:name/files on an unknown pack returns 404",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto sharedDataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir, sharedDataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/data-packs/no-such-pack/files");
  REQUIRE(res);
  CHECK(res->status == 404);

  std::filesystem::remove_all(dataDir);
  std::filesystem::remove_all(sharedDataDir);
}

TEST_CASE("POST /data-packs/:name/sync without a remote param returns 400, synchronously",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Post("/data-packs/my-pack/sync");
  REQUIRE(res);
  CHECK(res->status == 400);

  // No sync was ever started - sync-status should still show the untouched
  // default state.
  auto statusRes = client.Get("/data-packs/sync-status");
  REQUIRE(statusRes);
  CHECK(nlohmann::json::parse(statusRes->body).at("done") == true);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("POST /data-packs/:name/sync pulls files from a real remote server over HTTP",
          "[integration][data_packs]") {
  auto sourceDataDir = makeTempDir();
  auto sourceSharedDataDir = makeTempDir();
  writeFile(sourceSharedDataDir / "my-pack" / "dictionaries" / "en-word.json",
            "{\"word\":\"word\"}");
  writeFile(sourceSharedDataDir / "my-pack" / "dictionaries" / "en-us-word.ogg", "fake audio");
  auto source = startTestServer(kTestPort, native_server_test::fixturesDir(), sourceDataDir,
                                 sourceSharedDataDir);

  auto pullerDataDir = makeTempDir();
  auto puller = startTestServer(kTestPort2, native_server_test::fixturesDir(), pullerDataDir);
  httplib::Client pullerClient(kTestHost, kTestPort2);

  auto startRes = pullerClient.Post(
      ("/data-packs/my-pack/sync?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(startRes);
  REQUIRE(startRes->status == 200);

  auto finalStatus = waitForSyncDone(pullerClient);
  CHECK(finalStatus.at("pack") == "my-pack");
  CHECK(finalStatus.at("total") == 2);
  CHECK(finalStatus.at("copied") == 2);
  CHECK(finalStatus.at("error").is_null());

  CHECK(std::filesystem::exists(pullerDataDir / "server_data" / "dictionaries" / "en-word.json"));
  CHECK(
      std::filesystem::exists(pullerDataDir / "server_data" / "dictionaries" / "en-us-word.ogg"));

  std::filesystem::remove_all(sourceDataDir);
  std::filesystem::remove_all(sourceSharedDataDir);
  std::filesystem::remove_all(pullerDataDir);
}

TEST_CASE("POST /data-packs/:name/sync against an unreachable remote reports an error via "
          "sync-status, not an immediate failure",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  // Nothing is listening on kTestPort2 in this test.
  auto startRes = client.Post(
      ("/data-packs/my-pack/sync?remote=" + remoteAddress(kTestPort2)).c_str());
  REQUIRE(startRes);
  REQUIRE(startRes->status == 200);

  auto finalStatus = waitForSyncDone(client);
  CHECK_FALSE(finalStatus.at("error").is_null());

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("POST /data-packs/:name/sync against a remote with no such pack reports an error via "
          "sync-status",
          "[integration][data_packs]") {
  auto sourceDataDir = makeTempDir();
  auto sourceSharedDataDir = makeTempDir();
  writeFile(sourceSharedDataDir / "real-pack" / "file.json", "{}");
  auto source = startTestServer(kTestPort, native_server_test::fixturesDir(), sourceDataDir,
                                 sourceSharedDataDir);

  auto pullerDataDir = makeTempDir();
  auto puller = startTestServer(kTestPort2, native_server_test::fixturesDir(), pullerDataDir);
  httplib::Client pullerClient(kTestHost, kTestPort2);

  auto startRes = pullerClient.Post(
      ("/data-packs/no-such-pack/sync?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(startRes);
  REQUIRE(startRes->status == 200);

  auto finalStatus = waitForSyncDone(pullerClient);
  CHECK_FALSE(finalStatus.at("error").is_null());

  std::filesystem::remove_all(sourceDataDir);
  std::filesystem::remove_all(sourceSharedDataDir);
  std::filesystem::remove_all(pullerDataDir);
}

TEST_CASE("POST /data-packs/:name/sync while one is already running returns 409",
          "[integration][data_packs]") {
  auto sourceDataDir = makeTempDir();
  auto sourceSharedDataDir = makeTempDir();
  // Enough files that the first sync is still running when the second
  // request lands - a handful of files is plenty given the poll below has
  // no delay before firing the second request.
  for (int i = 0; i < 300; ++i) {
    writeFile(sourceSharedDataDir / "big-pack" / (std::to_string(i) + ".json"), "{}");
  }
  auto source = startTestServer(kTestPort, native_server_test::fixturesDir(), sourceDataDir,
                                 sourceSharedDataDir);

  auto pullerDataDir = makeTempDir();
  auto puller = startTestServer(kTestPort2, native_server_test::fixturesDir(), pullerDataDir);
  httplib::Client pullerClient(kTestHost, kTestPort2);

  auto firstRes = pullerClient.Post(
      ("/data-packs/big-pack/sync?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(firstRes);
  REQUIRE(firstRes->status == 200);

  auto secondRes = pullerClient.Post(
      ("/data-packs/big-pack/sync?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(secondRes);
  CHECK(secondRes->status == 409);

  waitForSyncDone(pullerClient);
  std::filesystem::remove_all(sourceDataDir);
  std::filesystem::remove_all(sourceSharedDataDir);
  std::filesystem::remove_all(pullerDataDir);
}

TEST_CASE("POST /data-packs/:name/sync works self-targeting, over a real loopback round-trip",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto sharedDataDir = makeTempDir();
  writeFile(sharedDataDir / "my-pack" / "a.json", "12345");  // 5 bytes
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir, sharedDataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto startRes =
      client.Post(("/data-packs/my-pack/sync?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(startRes);
  REQUIRE(startRes->status == 200);

  auto finalStatus = waitForSyncDone(client);
  CHECK(finalStatus.at("copied") == 1);
  CHECK(finalStatus.at("error").is_null());
  CHECK(std::filesystem::exists(dataDir / "server_data" / "a.json"));

  std::filesystem::remove_all(dataDir);
  std::filesystem::remove_all(sharedDataDir);
}

TEST_CASE("POST /data-packs/clear wipes server_data but leaves the directory itself",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  writeFile(dataDir / "server_data" / "dictionaries" / "en-word.json", "{}");
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Post("/data-packs/clear");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  CHECK(std::filesystem::is_directory(dataDir / "server_data"));
  CHECK(std::filesystem::is_empty(dataDir / "server_data"));

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /data-packs/sync-status before any sync ever ran reports done with no pack",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/data-packs/sync-status");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto body = nlohmann::json::parse(res->body);
  CHECK(body.at("done") == true);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /data-packs/server-data-size returns 0 before anything is synced",
          "[integration][data_packs]") {
  auto dataDir = makeTempDir();
  auto server = startTestServer(kTestPort, native_server_test::fixturesDir(), dataDir);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/data-packs/server-data-size");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto body = nlohmann::json::parse(res->body);
  CHECK(body.at("bytes") == 0);

  std::filesystem::remove_all(dataDir);
}

TEST_CASE("GET /data-packs/server-data-size reflects real file sizes after a sync",
          "[integration][data_packs]") {
  auto sourceDataDir = makeTempDir();
  auto sourceSharedDataDir = makeTempDir();
  writeFile(sourceSharedDataDir / "my-pack" / "a.json", "12345");  // 5 bytes
  auto source = startTestServer(kTestPort, native_server_test::fixturesDir(), sourceDataDir,
                                 sourceSharedDataDir);

  auto pullerDataDir = makeTempDir();
  auto puller = startTestServer(kTestPort2, native_server_test::fixturesDir(), pullerDataDir);
  httplib::Client pullerClient(kTestHost, kTestPort2);

  auto startRes = pullerClient.Post(
      ("/data-packs/my-pack/sync?remote=" + remoteAddress(kTestPort)).c_str());
  REQUIRE(startRes);
  REQUIRE(startRes->status == 200);
  waitForSyncDone(pullerClient);

  auto res = pullerClient.Get("/data-packs/server-data-size");
  REQUIRE(res);
  REQUIRE(res->status == 200);
  auto body = nlohmann::json::parse(res->body);
  CHECK(body.at("bytes") == 5);

  std::filesystem::remove_all(sourceDataDir);
  std::filesystem::remove_all(sourceSharedDataDir);
  std::filesystem::remove_all(pullerDataDir);
}
