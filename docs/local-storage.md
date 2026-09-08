# Local Storage (`StorageMap`, `ServerData`)

Two local (per-device) storage systems, for two different data shapes, both
backed by `native-server` (the same C++ server that serves this app's own
files and YouTube captions - see `native-server/README.md`) instead of the
browser's `localStorage`:

- **`StorageMap`** - small key→JSON-value data the client both reads and
  writes (settings, "where you left off" - the one existing example being
  the sync server URL, see `docs/setup-cloudflare.md`).
- **`ServerData`** - arbitrary files that `native-server` itself writes
  (e.g. a downloaded/cached file) and the web app only ever reads back, via
  a plain static HTTP route - no client write path at all. See its own
  section below.

Neither is to be confused with the [Online shared
storage](common-design-philosophy.md#online-shared-storage) pattern
(Cloudflare Worker + KV) - that's for data that should look the same across
devices; both of these are for data that lives on one device only.

## Why not just `localStorage`

`localStorage` works fine inside a single `WKWebView`/browser tab, but this
app runs the *same* frontend across multiple genuinely different
environments - the CLI-served page and the Mac/iOS app's embedded
`WKWebView` - and `localStorage` is scoped per origin/webview, not shared
or backed up in any useful way on-device.
Routing small persistent data through `native-server` instead means it
lives in one real file, next to (and readable/inspectable the same way as)
everything else the server already manages, and is included in the OS's
normal app-data backup story (Application Support on macOS/iOS is included
in backups by default) rather than living in whatever ad hoc place
`WKWebView`'s `localStorage` implementation happens to keep it.

## Layers, top to bottom

### 1. Frontend: `src/engine/storageMap.js`

```js
import { StorageMap } from './storageMap.js'
const map = StorageMap.get('core')       // any string mapId
await map.set('sync-server-url', 'https://...')
await map.get('sync-server-url')          // -> the value, or null if absent
await map.delete('sync-server-url')
```

- One map is a flat, independent key→JSON-value namespace, named by an
  arbitrary `mapId` string. Multiple maps just means multiple independent
  namespaces (currently only `'core'` exists - see `syncConfig.js`) - no
  registration step, a map springs into existence on its first write.
- Values are arbitrary JSON (objects, arrays, strings, numbers, booleans) -
  no manual `JSON.stringify`/`parse` at call sites, unlike raw
  `localStorage`.
- **Always backed by `native-server`** - every real usage pattern (the
  native Mac/iOS app, or the web app served locally via `native_server_cli`)
  has native-server serving the page in the first place, so it's always
  reachable; there's no `localStorage` fallback to fall back to. No
  dual-write, no client-side cache of server values - each call is a real
  round trip, so the server's file is always the live source of truth from
  the frontend's point of view.
- Best-effort on failure: a failed request/parse logs (via `appLog.js`,
  see the in-app Logs screen under Settings) and resolves to `null`/does
  nothing, rather than throwing. Nothing in the app should ever be
  unusable because this layer failed.

### 2. Server routes: `native_server_core/lib/storage_route.cpp`

`GET`/`PUT`/`DELETE /storage/maps/:mapId/:key`, registered by
`registerStorageRoutes()` (called from `server.cpp`'s `registerRoutes()`).

- `GET` → `{"value": ...}`, or `404 {"error": "Key not found"}` if absent.
- `PUT` → body must be `{"value": <any JSON>}`; `400` on malformed JSON or
  a missing `value` field; `200 {"status": "ok"}` otherwise.
- `DELETE` → idempotent, always `200 {"status": "ok"}` whether or not the
  key existed.
- A map is created transparently on its first `PUT` - no separate
  "create map" call from any layer above this one.

### 3. Storage engine: `native-server/storage_map/`

`StorageMap` (one JSON file per map) + `StorageMapRegistry` (resolves a
`mapId` string to its `StorageMap`, creating it - and the `storage_map/`
subdirectory, if needed - on first access, via `try_emplace` so it's safe
to call from multiple threads for different `mapId`s concurrently).

- Each map is lazy-loaded from `<dataDir>/storage_map/<mapId>.json` on
  first access, and whole-file-overwritten on every mutation (`set`/
  `remove`) - simple, correct for this data's size (small, infrequent
  writes), not optimized for high write volume.
- Both classes guard their state with their own `std::mutex` -
  `native_server_core` dispatches requests across an 8-thread pool, so
  concurrent requests against the same map (or different maps racing to
  create their registry entry) both need explicit protection; this isn't
  optional given the server's threading model.
- `StorageMapError` (a `std::runtime_error`) is thrown only for a genuinely
  malformed on-disk file or an unwritable path - not for a missing key,
  which is a normal, expected `std::nullopt` return from `get()`.

### 4. Where `dataDir` actually points

`StorageMapRegistry` is constructed with a `dataDir` (`ServerOptions::dataDir`,
passed through the C API's `native_server_create(..., dataDir)` param), and
that root differs by how the server is running:

- **CLI** (`native_server_cli`): `.app_data`, relative to the CWD the CLI
  is invoked from (resolved to an absolute path in `main.cpp`) - i.e. the
  repo root when run as documented. Override with `--data-dir` or
  `NATIVE_SERVER_DATA_DIR`. Gitignored.
- **Mac/iOS app**: `Application Support/NativeServerData/` inside the app's
  own sandboxed container (`FileManager.default.urls(for:
  .applicationSupportDirectory, in: .userDomainMask)`, created on first
  launch if missing - see `english_practice_appApp.swift`). Same API on
  both platforms; the container itself is keyed by bundle identifier on
  macOS (stable, Finder/Terminal-browsable) and by a UUID on iOS (not
  user-browsable without Xcode's "Download Container..."). Survives normal
  app rebuild/reinstall-in-place (same bundle ID); see the root
  `README.md`'s "Data storage location" section for the short version
  users see.

Either way, the actual on-disk shape is identical: `<dataDir>/storage_map/
<mapId>.json`, one file per map, e.g. `.app_data/storage_map/core.json`
for the CLI.

## `ServerData`

The asymmetric counterpart to `StorageMap`: files that `native-server`
itself writes directly (not via any client-facing write API) and the web
app only ever reads back, over a plain static HTTP route - not a
JSON-key-value API like `StorageMap`'s.

### 1. Storage engine: `native-server/server_data/`

`ServerDataStore`, constructed with a base directory:

- `bool exists(relativePath) const`
- `void write(relativePath, bytes) const` - creates parent directories as
  needed, and writes atomically (temp file in the same directory, then
  `std::filesystem::rename`) so a concurrent HTTP reader hitting the static
  route (below) never sees a partially-written file. Always overwrites -
  `ServerDataStore` itself has no cache policy of its own; a caller wanting
  "only download once" checks `exists()` first and decides for itself.
- `resolve(relativePath) const` - the absolute filesystem path a relative
  path maps to, exposed for callers that need it (e.g. to check size/mtime)
  without re-deriving `<dataDir>/server_data/<relativePath>` themselves.
- Rejects any relative path that's absolute, or contains a `..` segment -
  a plain `baseDir / relativePath` join would otherwise let an absolute
  `relativePath` silently discard `baseDir` entirely (documented
  `std::filesystem::path` behavior), or let `..` escape the intended tree.
- `ServerDataStore::write()` is a convenience, not a mandatory gate - other
  server-side code is free to write under `<dataDir>/server_data/` with
  plain C/C++ file APIs directly if that's more convenient; nothing enforces
  going through this class.
- Covered by `native-server/tests/unit/test_server_data_store.cpp`
  (round-trip, nested directory creation, overwrite, path-traversal/
  absolute-path rejection, and that only the final file is ever visible on
  disk after a write - never a leftover `.tmp*`).

### 2. Reading it back: a static HTTP mount

**Not yet wired up as of this writing** - the plan is a plain
`svr.set_mount_point("/server_data", (dataDir / "server_data").string())`
in `server.cpp`'s `registerRoutes()`, right alongside the existing `"/"` →
`dist/` mount (same cpp-httplib mechanism, no new route-handling code).
Once added, the web app reads a cached file by just using
`/server_data/<relativePath>` directly as a URL (e.g. an `<audio src>`) -
no fetch, no JSON parsing, no JS client needed, since there's nothing to
write from that side.

### On-disk location

Same `dataDir` as `StorageMap` (see below) - `<dataDir>/server_data/
<relativePath>`, sibling to `<dataDir>/storage_map/`.

## TODO: `StorageDatabase` / `StorageFileSystem` (not implemented)

`StorageMap` and `ServerData` were two of three originally-planned local
storage shapes; the third remains an idea, not an implementation:

- **`StorageDatabase`** - a bigger, more structured store for data that
  outgrows a flat key→JSON-value map (real querying, not just point
  lookups by key). No `localStorage` fallback planned - unlike
  `StorageMap`, there's no reasonable way to emulate this shape on top of
  `localStorage`, so it would simply be unavailable when `native-server`
  isn't reachable.

Don't confuse this with `ServerData` above, despite the similar-sounding
original name (`StorageFileSystem`): `StorageDatabase` would still be a
client-read/write API (like `StorageMap`, just more structured), whereas
`ServerData` is deliberately asymmetric - only `native-server` itself
writes, the client only ever reads. `ServerData` was built because a real,
narrower need for that asymmetric shape came up (see
`docs/dictionary-spec.md` once it's finalized); a generic client-read/write
file API remains unbuilt, and should follow `StorageMap`'s four-layer shape
if it's ever actually needed.

## Current usage

Two maps exist today:

- Map `'core'`, key `'sync-server-url'` (the Cloudflare Worker URL - see
  `src/engine/syncConfig.js` and `docs/setup-cloudflare.md`).
  `syncConfig.js` layers an in-memory cache on top of `StorageMap` (warmed
  on module load) purely because `ytHistory.js`'s `sendHistoryBeacon()`
  needs synchronous access during page unload (`navigator.sendBeacon` must
  be called non-awaited) - `StorageMap` itself is deliberately async-only,
  with no synchronous/cached path of its own.
- Map `'flashcards'` (`src/engine/flashcardsOfflineCache.js`) - a
  read-only offline fallback for Flashcards, keyed by `'set-list'` (the
  set name list) and `` `set:${name}` `` per set (just `{uid, front,
  back}` per card, no scheduling fields). The DB is always tried first;
  this is only ever read on a genuine connectivity failure. See
  `docs/flashcards-spec.md`'s "Offline read-only cache" section for the
  full design.

Any future setting that needs to persist per-device (not synced, per
[Online shared storage](common-design-philosophy.md#online-shared-storage))
should reach for `StorageMap.get(<mapId>)` directly rather than adding
another one-off `localStorage` call.

`ServerData` has no wired-up consumer yet as of this writing - it's built
and tested, but nothing calls `ServerDataStore::write()` in production code
yet, and the static route to read files back doesn't exist yet either (see
above).

## File map

- `src/engine/storageMap.js` - the frontend client (`StorageMap.get(mapId)` →
  `get`/`set`/`delete`)
- `src/engine/syncConfig.js` - the one real caller so far, plus its
  synchronous-cache wrapper
- `native-server/native_server_core/lib/storage_route.h`/`.cpp` - the HTTP
  routes
- `native-server/storage_map/include/storage_map/storage_map.h`,
  `native-server/storage_map/lib/storage_map.cpp` - `StorageMap`/
  `StorageMapRegistry`
- `native-server/tests/unit/test_storage_map.cpp`,
  `native-server/tests/integration/test_storage_route.cpp` - Catch2
  coverage for both layers
- `native-server/server_data/include/server_data/server_data_store.h`,
  `native-server/server_data/lib/server_data_store.cpp` - `ServerDataStore`
- `native-server/tests/unit/test_server_data_store.cpp` - its Catch2
  coverage
- `english-practice-app/english-practice-app/english_practice_appApp.swift` -
  where the Mac/iOS app resolves and creates its `dataDir`
- `native-server/native_server_cli_lib/lib/cli_config.cpp`,
  `native-server/native_server_cli/main.cpp` - where the CLI resolves its
  `dataDir`
