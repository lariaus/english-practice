#include "data_packs/data_packs.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace data_packs {

std::vector<std::string> listPacks(const std::filesystem::path& sharedDataDir) {
  std::vector<std::string> packs;

  std::error_code existsEc;
  if (sharedDataDir.empty() || !std::filesystem::is_directory(sharedDataDir, existsEc) ||
      existsEc) {
    return packs;
  }

  for (const auto& entry : std::filesystem::directory_iterator(sharedDataDir)) {
    if (entry.is_directory()) {
      packs.push_back(entry.path().filename().string());
    }
  }

  std::sort(packs.begin(), packs.end());
  return packs;
}

std::vector<std::string> listPackFiles(const std::filesystem::path& sharedDataDir,
                                        const std::string& packName) {
  std::filesystem::path source = sharedDataDir / packName;
  std::vector<std::string> files;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
    if (entry.is_regular_file()) {
      files.push_back(std::filesystem::relative(entry.path(), source).string());
    }
  }

  return files;
}

void syncPack(const std::function<std::vector<std::string>()>& listFiles,
              const std::function<std::string(const std::string&)>& fetchBytes,
              const std::filesystem::path& destDir,
              const std::function<void(std::size_t, std::size_t)>& onProgress) {
  std::vector<std::string> files = listFiles();
  std::size_t total = files.size();
  std::size_t copied = 0;
  if (onProgress) onProgress(copied, total);

  for (const auto& relative : files) {
    std::string bytes = fetchBytes(relative);

    std::filesystem::path destPath = destDir / relative;
    std::filesystem::create_directories(destPath.parent_path());

    std::ofstream out(destPath, std::ios::binary);
    if (!out) {
      throw std::runtime_error("Failed to open destination file: " + destPath.string());
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
      throw std::runtime_error("Failed to write destination file: " + destPath.string());
    }
    out.close();

    ++copied;
    if (onProgress) onProgress(copied, total);
  }
}

void clearServerData(const std::filesystem::path& destDir) {
  std::error_code existsEc;
  if (!std::filesystem::is_directory(destDir, existsEc) || existsEc) return;

  for (const auto& entry : std::filesystem::directory_iterator(destDir)) {
    std::filesystem::remove_all(entry.path());
  }
}

std::uintmax_t serverDataSizeBytes(const std::filesystem::path& destDir) {
  std::error_code existsEc;
  if (!std::filesystem::is_directory(destDir, existsEc) || existsEc) return 0;

  std::uintmax_t total = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(destDir)) {
    if (entry.is_regular_file()) total += entry.file_size();
  }
  return total;
}

}  // namespace data_packs
