#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace server_data {

class ServerDataError : public std::runtime_error {
 public:
  explicit ServerDataError(const std::string& message) : std::runtime_error(message) {}
};

// Files written directly by native-server itself (e.g. a downloaded
// dictionary audio clip, or a cached dictionary lookup result) and served
// back to the web app read-only via a plain static HTTP mount - see
// docs/local-storage.md's "StorageFileSystem" TODO section, which this
// deliberately does NOT implement (no generic list/read/write API for
// client use) - this is narrower and simpler: the server writes, the client
// only ever reads via the static route.
//
// Distinct from StorageMap (a client-read/write JSON key-value store) -
// ServerData holds arbitrary binary files with no client write path at all.
//
// Thread-safe with respect to concurrent writes to *different* relative
// paths (each write is independent). A write is atomic (temp file + rename)
// so a concurrent HTTP reader hitting the static mount never sees a
// partially-written file.
class ServerDataStore {
 public:
  explicit ServerDataStore(std::filesystem::path baseDir);

  bool exists(const std::string& relativePath) const;

  // Creates parent directories as needed. Always overwrites - callers
  // decide their own cache policy (e.g. checking exists() first) rather
  // than this class enforcing one. Throws ServerDataError on any I/O
  // failure, or if relativePath attempts to escape baseDir (a `..`
  // segment).
  void write(const std::string& relativePath, const std::string& bytes) const;

  // Returns the file's contents, or nullopt if it doesn't exist - a normal,
  // expected outcome (a cache miss), not an error. Throws ServerDataError
  // only on an actual I/O failure, or if relativePath attempts to escape
  // baseDir. Server-side code reading its own previously-written cache
  // (e.g. a cached dictionary lookup) is the intended use - the web app
  // itself still only ever reads via the static route, never this method.
  std::optional<std::string> read(const std::string& relativePath) const;

  // The absolute filesystem path a relative path resolves to - exposed so
  // callers can e.g. check size/mtime, but writing should still go through
  // write() above for the atomicity guarantee.
  std::filesystem::path resolve(const std::string& relativePath) const;

 private:
  std::filesystem::path _baseDir;
};

}  // namespace server_data
