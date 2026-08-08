# Local Storage (`StorageMap`)

A key→JSON-value storage system for small, per-device app data (settings,
"where you left off," the one existing example being the sync server URL -
see `docs/setup-cloudflare.md`). Not to be confused with the [Online shared
storage](common-design-philosophy.md#online-shared-storage) pattern
(Cloudflare Worker + KV) - that's for data that should look the same across
devices; this is for data that lives on one device only, backed by
`native-server` (the same C++ server that serves this app's own files and
YouTube captions - see `native-server/README.md`) instead of the browser's
`localStorage`, wherever `native-server` is actually running the page.

## Why not just `localStorage`

`localStorage` works fine inside a single `WKWebView`/browser tab, but this
app runs the *same* frontend across multiple genuinely different
environments - a plain browser tab (GitHub Pages), the CLI-served page, and
the Mac/iOS app's embedded `WKWebView` - and `localStorage` is scoped per
origin/webview, not shared or backed up in any useful way on-device.
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
- **Exactly one backing store is active per session**, decided once:
  `isNativeServerAvailable()` (`nativeServerClient.js`) does a single `GET
  /health` check, cached for the app's lifetime and shared with the
  captions feature. If it succeeds, every `StorageMap` call for the rest of
  the session goes over the network to `native-server`. If it fails (e.g.
  plain GitHub Pages with no server at all), every call falls back to
  `localStorage` instead, for the whole session. There's no dual-write, no
  client-side cache of server values, and no switching mid-session - each
  call is a real round trip when the server is available, so the server's
  file is always the live source of truth from the frontend's point of
  view.
- Both paths are best-effort: a failed request/parse logs (via
  `appLog.js`, see the in-app Logs screen under Settings) and resolves to
  `null`/does nothing, rather than throwing. Nothing in the app should ever
  be unusable because this layer failed.

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

## TODO: `StorageDatabase` / `StorageFileSystem` (not implemented)

`StorageMap` was originally planned as one of three local storage shapes;
only it was actually built so far, since it's the only one any current
feature needs. The other two remain ideas, not implementations:

- **`StorageDatabase`** - a bigger, more structured store for data that
  outgrows a flat key→JSON-value map (real querying, not just point
  lookups by key). No `localStorage` fallback planned - unlike
  `StorageMap`, there's no reasonable way to emulate this shape on top of
  `localStorage`, so it would simply be unavailable when `native-server`
  isn't reachable.
- **`StorageFileSystem`** - a real filesystem-like API (list a directory,
  list files, read/write a file) for on-device files rather than JSON
  values. Same deal - no `localStorage` fallback, unavailable without
  `native-server`.

Neither has a `dataDir` layout, server route, or JS client yet - if/when
one is actually needed, it should follow the same four-layer shape as
`StorageMap` above (independent C++ lib + unit tests, server route + tests,
JS client), living alongside `storage_map/` rather than inside it.

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

## File map

- `src/engine/storageMap.js` - the frontend client (`StorageMap.get(mapId)` →
  `get`/`set`/`delete`), and its `localStorage` fallback
- `src/engine/nativeServerClient.js` - `isNativeServerAvailable()`, the
  cached, shared reachability check
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
- `english-practice-app/english-practice-app/english_practice_appApp.swift` -
  where the Mac/iOS app resolves and creates its `dataDir`
- `native-server/native_server_cli_lib/lib/cli_config.cpp`,
  `native-server/native_server_cli/main.cpp` - where the CLI resolves its
  `dataDir`
