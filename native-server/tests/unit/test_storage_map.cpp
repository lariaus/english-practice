#include <storage_map/storage_map.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>

namespace {

std::filesystem::path makeTempDir() {
  auto base = std::filesystem::temp_directory_path();
  std::random_device rd;
  auto dir = base / ("storage_map_test_" + std::to_string(rd()));
  std::filesystem::create_directories(dir);
  return dir;
}

}  // namespace

TEST_CASE("get returns nullopt for a missing key", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMap map(dir / "test.json");

  REQUIRE_FALSE(map.get("missing").has_value());

  std::filesystem::remove_all(dir);
}

TEST_CASE("set then get round-trips a value", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMap map(dir / "test.json");

  map.set("foo", nlohmann::json::array({1, 2, 3}));
  auto value = map.get("foo");

  REQUIRE(value.has_value());
  REQUIRE(*value == nlohmann::json::array({1, 2, 3}));

  std::filesystem::remove_all(dir);
}

TEST_CASE("remove deletes a key", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMap map(dir / "test.json");

  map.set("foo", "bar");
  map.remove("foo");

  REQUIRE_FALSE(map.get("foo").has_value());

  std::filesystem::remove_all(dir);
}

TEST_CASE("remove is idempotent for a missing key", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMap map(dir / "test.json");

  REQUIRE_NOTHROW(map.remove("missing"));

  std::filesystem::remove_all(dir);
}

TEST_CASE("data persists across separate StorageMap instances pointed at the same file",
          "[storage_map]") {
  auto dir = makeTempDir();
  auto filePath = dir / "test.json";

  {
    storage_map::StorageMap map(filePath);
    map.set("foo", 42);
  }

  storage_map::StorageMap map2(filePath);
  auto value = map2.get("foo");

  REQUIRE(value.has_value());
  REQUIRE(*value == 42);

  std::filesystem::remove_all(dir);
}

TEST_CASE("various JSON value shapes round-trip exactly", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMap map(dir / "test.json");

  map.set("string", "hello");
  map.set("number", 3.14);
  map.set("array", nlohmann::json::array({1, "two", 3.0}));
  map.set("object", nlohmann::json{{"nested", true}});
  map.set("null", nlohmann::json());

  REQUIRE(*map.get("string") == "hello");
  REQUIRE(*map.get("number") == 3.14);
  REQUIRE(*map.get("array") == nlohmann::json::array({1, "two", 3.0}));
  REQUIRE(*map.get("object") == nlohmann::json{{"nested", true}});
  REQUIRE(map.get("null")->is_null());

  std::filesystem::remove_all(dir);
}

TEST_CASE("StorageMapRegistry creates a new map file on first write", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMapRegistry registry(dir);

  registry.getOrCreate("accounts").set("foo", "bar");

  REQUIRE(std::filesystem::exists(dir / "storage_map" / "accounts.json"));

  std::filesystem::remove_all(dir);
}

TEST_CASE("StorageMapRegistry reuses the same map for repeat calls", "[storage_map]") {
  auto dir = makeTempDir();
  storage_map::StorageMapRegistry registry(dir);

  auto& map1 = registry.getOrCreate("accounts");
  map1.set("foo", "bar");
  auto& map2 = registry.getOrCreate("accounts");

  REQUIRE(&map1 == &map2);
  REQUIRE(map2.get("foo").has_value());

  std::filesystem::remove_all(dir);
}
