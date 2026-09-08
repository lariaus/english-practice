#include "data_packs/data_packs.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

using data_packs::clearServerData;
using data_packs::listPackFiles;
using data_packs::listPacks;
using data_packs::serverDataSizeBytes;
using data_packs::syncPack;

namespace {

// A fresh temp directory per test, cleaned up on scope exit - matching
// test_server_data_store.cpp's own convention.
class TempDir {
 public:
  TempDir() {
    _path = std::filesystem::temp_directory_path() /
            ("data_packs_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(_path);
  }
  ~TempDir() { std::filesystem::remove_all(_path); }

  const std::filesystem::path& path() const { return _path; }

 private:
  std::filesystem::path _path;
};

void writeFile(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file << contents;
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

}  // namespace

TEST_CASE("listPacks returns nothing for a directory that doesn't exist", "[data_packs]") {
  TempDir dir;
  CHECK(listPacks(dir.path() / "does-not-exist").empty());
}

TEST_CASE("listPacks returns nothing for an empty sharedDataDir", "[data_packs]") {
  TempDir dir;
  CHECK(listPacks(dir.path()).empty());
}

TEST_CASE("listPacks returns nothing for an empty path", "[data_packs]") {
  CHECK(listPacks(std::filesystem::path{}).empty());
}

TEST_CASE("listPacks lists every top-level directory, sorted, ignoring plain files",
          "[data_packs]") {
  TempDir dir;
  std::filesystem::create_directories(dir.path() / "words-list-top-20k");
  std::filesystem::create_directories(dir.path() / "words-list-top-1k");
  writeFile(dir.path() / "README.txt", "not a pack");

  auto packs = listPacks(dir.path());
  REQUIRE(packs.size() == 2);
  CHECK(packs[0] == "words-list-top-1k");
  CHECK(packs[1] == "words-list-top-20k");
}

TEST_CASE("listPackFiles lists every nested file, relative to the pack root", "[data_packs]") {
  TempDir dir;
  writeFile(dir.path() / "my-pack" / "dictionaries" / "wiktionaryapi" / "en-word.json", "{}");
  writeFile(dir.path() / "my-pack" / "dictionaries" / "wiktionaryapi" / "en-us-word.ogg", "audio");

  auto files = listPackFiles(dir.path(), "my-pack");
  std::sort(files.begin(), files.end());
  REQUIRE(files.size() == 2);
  CHECK(files[0] == "dictionaries/wiktionaryapi/en-us-word.ogg");
  CHECK(files[1] == "dictionaries/wiktionaryapi/en-word.json");
}

TEST_CASE("listPackFiles throws on an unknown pack", "[data_packs]") {
  TempDir dir;
  REQUIRE_THROWS_AS(listPackFiles(dir.path(), "no-such-pack"), std::filesystem::filesystem_error);
}

TEST_CASE("syncPack copies every file via the injected fetch, preserving relative structure",
          "[data_packs]") {
  std::map<std::string, std::string> remoteFiles = {
      {"dictionaries/wiktionaryapi/en-word.json", "{\"word\":\"word\"}"},
      {"dictionaries/wiktionaryapi/en-us-word.ogg", "fake audio"},
  };
  std::vector<std::string> relPaths;
  for (const auto& [path, _] : remoteFiles) relPaths.push_back(path);

  TempDir dest;
  std::vector<std::pair<std::size_t, std::size_t>> progressCalls;
  syncPack([&relPaths]() { return relPaths; },
           [&remoteFiles](const std::string& relative) { return remoteFiles.at(relative); },
           dest.path(), [&progressCalls](std::size_t copied, std::size_t total) {
             progressCalls.push_back({copied, total});
           });

  CHECK(readFile(dest.path() / "dictionaries" / "wiktionaryapi" / "en-word.json") ==
        "{\"word\":\"word\"}");
  CHECK(readFile(dest.path() / "dictionaries" / "wiktionaryapi" / "en-us-word.ogg") ==
        "fake audio");

  // First call reports the real total with nothing copied yet (so a poller
  // sees "0 of 2" rather than "0 of 0" while the first file is in flight),
  // then one call per file, strictly increasing, every call carrying the
  // same total.
  REQUIRE(progressCalls.size() == 3);
  CHECK(progressCalls[0] == std::make_pair(std::size_t{0}, std::size_t{2}));
  CHECK(progressCalls[1].second == 2);
  CHECK(progressCalls[2] == std::make_pair(std::size_t{2}, std::size_t{2}));
}

TEST_CASE("syncPack creates the destination directory tree from scratch", "[data_packs]") {
  std::vector<std::string> relPaths = {"a.json"};

  // dest doesn't exist at all yet - the first-sync-ever case.
  std::filesystem::path dest =
      std::filesystem::temp_directory_path() / ("data_packs_dest_" + std::to_string(std::rand()));
  REQUIRE_FALSE(std::filesystem::exists(dest));

  syncPack([&relPaths]() { return relPaths; },
           [](const std::string&) { return std::string("1"); }, dest, nullptr);

  CHECK(readFile(dest / "a.json") == "1");
  std::filesystem::remove_all(dest);
}

TEST_CASE("syncPack overwrites a file that already exists at the destination", "[data_packs]") {
  TempDir dest;
  writeFile(dest.path() / "a.json", "stale content");
  std::vector<std::string> relPaths = {"a.json"};

  syncPack([&relPaths]() { return relPaths; },
           [](const std::string&) { return std::string("new content"); }, dest.path(), nullptr);

  CHECK(readFile(dest.path() / "a.json") == "new content");
}

TEST_CASE("syncPack propagates a listFiles failure without writing anything", "[data_packs]") {
  TempDir dest;
  REQUIRE_THROWS_AS(
      syncPack([]() -> std::vector<std::string> { throw std::runtime_error("unreachable"); },
               [](const std::string&) { return std::string(); }, dest.path(), nullptr),
      std::runtime_error);
  CHECK(std::filesystem::is_empty(dest.path()));
}

TEST_CASE(
    "syncPack propagates a fetchBytes failure partway through, having written what it could",
    "[data_packs]") {
  TempDir dest;
  std::vector<std::string> relPaths = {"a.json", "b.json"};

  REQUIRE_THROWS_AS(syncPack(
                         [&relPaths]() { return relPaths; },
                         [](const std::string& relative) -> std::string {
                           if (relative == "b.json") throw std::runtime_error("network error");
                           return "1";
                         },
                         dest.path(), nullptr),
                     std::runtime_error);

  CHECK(readFile(dest.path() / "a.json") == "1");
  CHECK_FALSE(std::filesystem::exists(dest.path() / "b.json"));
}

TEST_CASE("clearServerData removes nested content but leaves the root directory itself",
          "[data_packs]") {
  TempDir dir;
  writeFile(dir.path() / "dictionaries" / "wiktionaryapi" / "en-word.json", "{}");
  writeFile(dir.path() / "top-level.json", "{}");

  clearServerData(dir.path());

  CHECK(std::filesystem::is_directory(dir.path()));
  CHECK(std::filesystem::is_empty(dir.path()));
}

TEST_CASE("clearServerData is a no-op on an already-empty directory", "[data_packs]") {
  TempDir dir;
  clearServerData(dir.path());
  CHECK(std::filesystem::is_directory(dir.path()));
}

TEST_CASE("clearServerData is a no-op on a directory that doesn't exist", "[data_packs]") {
  TempDir dir;
  auto missing = dir.path() / "does-not-exist";
  clearServerData(missing);
  CHECK_FALSE(std::filesystem::exists(missing));
}

TEST_CASE("serverDataSizeBytes returns 0 for a directory that doesn't exist", "[data_packs]") {
  TempDir dir;
  CHECK(serverDataSizeBytes(dir.path() / "does-not-exist") == 0);
}

TEST_CASE("serverDataSizeBytes returns 0 for an empty directory", "[data_packs]") {
  TempDir dir;
  CHECK(serverDataSizeBytes(dir.path()) == 0);
}

TEST_CASE("serverDataSizeBytes sums every regular file's size, recursively", "[data_packs]") {
  TempDir dir;
  writeFile(dir.path() / "a.json", "12345");                // 5 bytes
  writeFile(dir.path() / "nested" / "b.ogg", "1234567890");  // 10 bytes

  CHECK(serverDataSizeBytes(dir.path()) == 15);
}
