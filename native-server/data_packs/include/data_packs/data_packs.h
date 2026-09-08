#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace data_packs {

// Every top-level directory directly under sharedDataDir - each one a
// "pack" produced by dictionary-crawler's own @output_dir convention (e.g.
// shared_data/words-list-top-1k/). Not recursive - a pack's own internal
// tree (e.g. dictionaries/wiktionaryapi/...) is left untouched by this
// listing; only the top level, "which packs exist," is enumerated. Files
// sitting directly under sharedDataDir (not directories) are ignored.
// Returns an empty list if sharedDataDir doesn't exist at all - the normal
// case on a build where sharedDataDir was never configured (e.g. the
// Mac/iOS app), not an error.
std::vector<std::string> listPacks(const std::filesystem::path& sharedDataDir);

// Recursively walks sharedDataDir/packName and returns every regular file's
// path relative to the pack root (e.g.
// "dictionaries/wiktionaryapi/en-word.json"). Order is whatever the
// filesystem iteration happens to produce - not guaranteed stable, callers
// must only rely on "every file appears exactly once". Throws
// std::filesystem::filesystem_error if packName doesn't exist under
// sharedDataDir. Backs GET /data-packs/:name/files - what a remote puller
// asks for before fetching any bytes.
std::vector<std::string> listPackFiles(const std::filesystem::path& sharedDataDir,
                                        const std::string& packName);

struct SyncProgress {
  std::string pack;
  std::size_t total = 0;
  std::size_t copied = 0;
  bool done = true;
  std::optional<std::string> error;
};

// Pulls a pack over the network into destDir. Takes the actual network
// calls as two injected functions rather than an address, so this library
// never depends on HTTP/https_client itself - the real implementations
// (constructed in data_packs_route.cpp, using https_client::HttpClient
// against http://<remote>/... URLs) are the only place this whole feature
// touches the network. This keeps syncPack fully unit-testable with plain
// in-memory fakes instead of a real server.
//
//   listFiles  - returns every relative file path this pack has at the
//                remote (what GET /data-packs/:name/files would return).
//                Called once, up front - its result's size becomes `total`.
//   fetchBytes - given one relative path, returns that file's raw bytes
//                (what a GET against the remote's
//                /shared_data/<pack>/<path> static mount would return).
//
// Both are expected to throw on failure (network error, remote 404, etc.) -
// syncPack itself has no knowledge of HTTP, sockets, or timeouts.
//
// Writes each file under destDir, preserving relative paths and overwriting
// anything already there, creating every directory it needs along the way -
// works correctly even before destDir exists on disk at all (the very
// first sync ever run).
//
// Calls onProgress(copiedSoFar, total) once immediately after listFiles()
// resolves (copiedSoFar == 0, so a poller sees the real total right away
// rather than "0 of 0"), then again after every file is written. Runs
// entirely on whatever thread calls it - this function has no threading of
// its own; the caller decides whether to run it on a background thread and
// how to publish onProgress's value.
//
// Propagates whatever listFiles/fetchBytes threw, or an error from the
// local write side, on the first failure rather than skipping and
// continuing - the walk stops there, so onProgress will have already
// reported however many files made it through before the failure.
void syncPack(const std::function<std::vector<std::string>()>& listFiles,
              const std::function<std::string(const std::string& relativePath)>& fetchBytes,
              const std::filesystem::path& destDir,
              const std::function<void(std::size_t copiedSoFar, std::size_t total)>& onProgress);

// Deletes everything under destDir, but not destDir itself - a no-op
// (never throws) if destDir doesn't exist or is already empty.
void clearServerData(const std::filesystem::path& destDir);

// The total size, in bytes, of every regular file anywhere under destDir,
// recursive. Returns 0 if destDir doesn't exist - the normal case before
// the very first sync, not an error.
std::uintmax_t serverDataSizeBytes(const std::filesystem::path& destDir);

}  // namespace data_packs
