#include "server_data/server_data_store.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>

using server_data::ServerDataError;
using server_data::ServerDataStore;

namespace {

// A fresh temp directory per test, cleaned up on scope exit - real
// filesystem I/O, no mocking, matching this codebase's convention of
// testing StorageMap the same way.
class TempDir {
 public:
  TempDir() {
    _path = std::filesystem::temp_directory_path() /
            ("server_data_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(_path);
  }
  ~TempDir() { std::filesystem::remove_all(_path); }

  const std::filesystem::path& path() const { return _path; }

 private:
  std::filesystem::path _path;
};

std::string readFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

}  // namespace

TEST_CASE("write() then exists() round-trips a file", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  CHECK_FALSE(store.exists("dictionaries/word.ogg"));
  store.write("dictionaries/word.ogg", "fake audio bytes");
  CHECK(store.exists("dictionaries/word.ogg"));
  CHECK(readFile(store.resolve("dictionaries/word.ogg")) == "fake audio bytes");
}

TEST_CASE("read() returns a written file's contents", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  store.write("dictionaries/en-smart.json", "{\"word\":\"smart\"}");
  auto contents = store.read("dictionaries/en-smart.json");
  REQUIRE(contents.has_value());
  CHECK(*contents == "{\"word\":\"smart\"}");
}

TEST_CASE("read() returns nullopt for a missing file, not an error", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  CHECK_FALSE(store.read("dictionaries/never-written.json").has_value());
}

TEST_CASE("write() creates parent directories that don't exist yet", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  store.write("a/b/c/word.ogg", "nested");
  CHECK(store.exists("a/b/c/word.ogg"));
}

TEST_CASE("write() overwrites an existing file rather than erroring", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  store.write("word.ogg", "first");
  store.write("word.ogg", "second, longer content");
  CHECK(readFile(store.resolve("word.ogg")) == "second, longer content");
}

TEST_CASE("resolve() rejects a relative path containing '..'", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  REQUIRE_THROWS_AS(store.resolve("../../etc/passwd"), ServerDataError);
  REQUIRE_THROWS_AS(store.write("../escape.txt", "nope"), ServerDataError);
}

TEST_CASE("resolve() rejects an absolute relative path", "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  // A naive baseDir / relativePath join would silently discard baseDir
  // entirely here (std::filesystem::path's own documented behavior for an
  // absolute right-hand operand) - must be rejected explicitly instead.
  REQUIRE_THROWS_AS(store.resolve("/etc/passwd"), ServerDataError);
}

TEST_CASE("no partial file is ever visible under the final path - write is atomic",
          "[server_data]") {
  TempDir dir;
  ServerDataStore store(dir.path());

  store.write("word.ogg", "complete content");

  // Only the final file should exist - no leftover .tmp* files from the
  // atomic write's intermediate step.
  int entryCount = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir.path())) {
    (void)entry;
    ++entryCount;
  }
  CHECK(entryCount == 1);
  CHECK(readFile(store.resolve("word.ogg")) == "complete content");
}
