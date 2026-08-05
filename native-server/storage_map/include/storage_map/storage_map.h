#pragma once

#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace storage_map {

class StorageMapError : public std::runtime_error {
 public:
  explicit StorageMapError(const std::string& message) : std::runtime_error(message) {}
};

// A key->JSON-value map backed by one JSON file. Thread-safe (guards its
// own state with a mutex) - native_server_core's thread pool can dispatch
// concurrent requests against the same map.
class StorageMap {
 public:
  explicit StorageMap(std::filesystem::path filePath);

  std::optional<nlohmann::json> get(const std::string& key) const;
  void set(const std::string& key, const nlohmann::json& value);
  void remove(const std::string& key);

 private:
  void load() const;
  void flush() const;

  std::filesystem::path _filePath;
  mutable nlohmann::json _data;
  mutable bool _loaded = false;
  mutable std::mutex _mutex;
};

// Resolves a mapId to its StorageMap, creating the map (and
// storage_map/ subdirectory, if needed) on first access.
class StorageMapRegistry {
 public:
  explicit StorageMapRegistry(std::filesystem::path dataDir);
  StorageMap& getOrCreate(const std::string& mapId);

 private:
  std::filesystem::path _dataDir;
  std::unordered_map<std::string, StorageMap> _maps;
  std::mutex _mutex;
};

}  // namespace storage_map
