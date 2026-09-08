#include "server_data/server_data_store.h"

#include <atomic>
#include <fstream>
#include <sstream>
#include <utility>

namespace server_data {

namespace {

std::atomic<uint64_t> gTempFileCounter{0};

}  // namespace

ServerDataStore::ServerDataStore(std::filesystem::path baseDir) : _baseDir(std::move(baseDir)) {}

std::filesystem::path ServerDataStore::resolve(const std::string& relativePath) const {
  std::filesystem::path rel(relativePath);
  if (rel.is_absolute()) {
    throw ServerDataError("Relative path must not be absolute: " + relativePath);
  }
  for (const auto& part : rel) {
    if (part == "..") {
      throw ServerDataError("Relative path must not contain '..': " + relativePath);
    }
  }
  return _baseDir / rel;
}

bool ServerDataStore::exists(const std::string& relativePath) const {
  return std::filesystem::exists(resolve(relativePath));
}

std::optional<std::string> ServerDataStore::read(const std::string& relativePath) const {
  std::filesystem::path path = resolve(relativePath);
  if (!std::filesystem::exists(path)) return std::nullopt;

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw ServerDataError("Could not open file for reading: " + path.string());
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  if (file.bad()) {
    throw ServerDataError("Could not read file: " + path.string());
  }
  return ss.str();
}

void ServerDataStore::write(const std::string& relativePath, const std::string& bytes) const {
  std::filesystem::path finalPath = resolve(relativePath);
  std::filesystem::create_directories(finalPath.parent_path());

  std::filesystem::path tempPath = finalPath;
  tempPath += ".tmp" + std::to_string(gTempFileCounter.fetch_add(1));

  {
    std::ofstream file(tempPath, std::ios::binary);
    if (!file) {
      throw ServerDataError("Could not open temp file for writing: " + tempPath.string());
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
      throw ServerDataError("Could not write to temp file: " + tempPath.string());
    }
  }

  std::error_code ec;
  std::filesystem::rename(tempPath, finalPath, ec);
  if (ec) {
    std::filesystem::remove(tempPath);
    throw ServerDataError("Could not finalize write to " + finalPath.string() + ": " +
                           ec.message());
  }
}

}  // namespace server_data
