#include "storage_map/storage_map.h"

#include <fstream>

namespace storage_map {

StorageMap::StorageMap(std::filesystem::path filePath) : _filePath(std::move(filePath)) {}

void StorageMap::load() const {
  if (_loaded) return;

  if (std::filesystem::exists(_filePath)) {
    std::ifstream file(_filePath);
    try {
      file >> _data;
    } catch (const nlohmann::json::parse_error& e) {
      throw StorageMapError("Could not parse storage map file " + _filePath.string() + ": " +
                             e.what());
    }
    if (!_data.is_object()) {
      _data = nlohmann::json::object();
    }
  } else {
    _data = nlohmann::json::object();
  }

  _loaded = true;
}

void StorageMap::flush() const {
  std::filesystem::create_directories(_filePath.parent_path());
  std::ofstream file(_filePath);
  if (!file) {
    throw StorageMapError("Could not write storage map file: " + _filePath.string());
  }
  file << _data.dump(2);
}

std::optional<nlohmann::json> StorageMap::get(const std::string& key) const {
  std::lock_guard<std::mutex> lock(_mutex);
  load();
  auto it = _data.find(key);
  if (it == _data.end()) {
    return std::nullopt;
  }
  return *it;
}

void StorageMap::set(const std::string& key, const nlohmann::json& value) {
  std::lock_guard<std::mutex> lock(_mutex);
  load();
  _data[key] = value;
  flush();
}

void StorageMap::remove(const std::string& key) {
  std::lock_guard<std::mutex> lock(_mutex);
  load();
  _data.erase(key);
  flush();
}

StorageMapRegistry::StorageMapRegistry(std::filesystem::path dataDir) : _dataDir(std::move(dataDir)) {}

StorageMap& StorageMapRegistry::getOrCreate(const std::string& mapId) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _maps.find(mapId);
  if (it != _maps.end()) {
    return it->second;
  }

  std::filesystem::path filePath = _dataDir / "storage_map" / (mapId + ".json");
  auto [inserted, ok] = _maps.try_emplace(mapId, filePath);
  return inserted->second;
}

}  // namespace storage_map
