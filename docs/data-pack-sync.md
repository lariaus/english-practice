# Data pack sync (cross-device, over the local network)

**Status: design complete, not yet implemented.** A same-machine-only version
of this shipped earlier and is being retired in place - see "What's changing
from the shipped version" below.

A tool, living in `native-server` on every build (CLI, Mac app, iOS app):
list the crawl outputs sitting under a `shared_data` directory (each one a
"pack" - e.g. `shared_data/words-list-top-1k/`, produced by
`dictionary-crawler`) on whichever machine actually has them - always the
CLI/dev Mac today - and pull a chosen pack's contents, **over the local
network**, into `<dataDir>/server_data/` on *whichever device the app is
running on* - the same directory tree `ServerDataStore` already reads
dictionary cache files from (see `docs/local-storage.md`'s `ServerData`
section). This is how an iPhone (or a second Mac) gets a big, pre-crawled
dataset onto itself without going through the App Store, a cable, or the
Cloudflare Worker/KV sync tier - it just asks the Mac directly, over WiFi.

**What's actually inside a pack**: exactly a word-list's `@output_dir`
contents (e.g. `dictionaries/wiktionaryapi/...`,
`dictionaries/freedictionaryapi/...`) - nothing else. The word-list `.txt`
file itself and its `-status.json` crawl-progress sidecar
(`native-server/dictionary_utils/data/`) are siblings of `@output_dir`, not
inside it, so they're never part of a pack and never get synced - a pack is
purely the *data*, with no memory of which crawl run produced it or how
complete that run was.

## Why this exists, and what it's explicitly not

This is the piece of a larger goal - moving large, optional datasets (a 1k or
20k word pronunciation pack, hundreds of MB) onto a device without going
through the Cloudflare Worker/KV sync tier, which is explicitly the wrong
tool for this (`common-design-philosophy.md`'s "Online shared storage"
section: "not for anything sizeable... that belongs in proper server-side
storage, or in local/on-device storage, not here").

**The actual file transfer is always two `native-server` processes talking
to each other directly, C++ to C++, over plain HTTP** - never a browser
`fetch()` reaching a different origin, and so **never a CORS concern of any
kind**. The web app (Vue, running inside whichever device's own
`native-server` is serving it) only ever talks to *its own* local
`native-server`, same-origin, exactly like every other feature in this app -
it triggers a sync and polls for progress, nothing more. That local
`native-server` is the one that reaches out across the network to the other
device's `native-server`, using `https_client::HttpClient` (the same
Objective-C++/NSURLSession-backed client already used for the Wiktionary/
FreeDictionary API calls) - a plain outbound HTTP client call, indistinguishable
in kind from those existing calls, just pointed at another `native-server`
instead of a public dictionary API.

**Platform scope, confirmed explicitly**: `shared_data` only ever exists on
the CLI/dev machine - the Mac that runs `dictionary-crawler`. So
`sharedDataDir` (the "do I have packs to serve" question) only ever gets a
real value from the CLI's own config; the Swift/iOS bridge
(`capi.h`/`native_server_create`) is untouched by this work and always passes
nothing, same as before. But **every build - CLI, Mac app, iOS app - gets the
full pulling side of this feature**: point it at another device's address on
the same WiFi network and pull a pack from there. The one concrete scenario
this whole design is built around: an iPhone, connected to the same WiFi as a
Mac that's running `native-server` as the CLI with real crawled packs under
`shared_data`, types that Mac's address into the iPhone app's Data Packs
screen and pulls a pack down onto itself.

### What's changing from the shipped version

An earlier version of this feature shipped with a **local-disk-copy**
`syncPack` - `std::filesystem::copy_file` from this same machine's own
`shared_data` into this same machine's own `server_data`, no network
involved at all. That was scoped by mistake: the actual goal was always
cross-device transfer, and this fix corrects it. `syncPack` is being retired
and replaced outright with a network-based implementation - **not added
alongside the old one** - so there is exactly one code path to maintain and
test, ever. Even a same-machine sync (pointing a device at its own address,
useful for local testing) now goes over a real HTTP round-trip through
`127.0.0.1` rather than a filesystem copy - "wastefully" real, deliberately,
because maintaining two parallel implementations (one fast/local, one
network) for what's conceptually one operation is worse than one path that's
occasionally talking to itself.

## Where things live, precisely

Reusing the exact existing `dataDir` resolution (`docs/local-storage.md`
section 4) rather than a hardcoded path - **unchanged from the shipped
version**:

- **`server_data` destination**: `<dataDir>/server_data/` - always *this*
  device's own directory, regardless of where the pack's source is. Every
  device - CLI, Mac app, iOS app - has one of these, and every sync writes
  into its own.
- **`shared_data` source**: `ServerOptions::sharedDataDir` - populated only
  by the CLI (`--shared-data-dir`/`NATIVE_SERVER_SHARED_DATA_DIR`, defaulting
  to `"shared_data"`), empty on every other build. This is what makes a
  device answerable as a source at all - see "serving" below. Unchanged from
  the shipped version.
- **The remote address** (new concept, not a path): a `host:port` string
  (e.g. `192.168.1.5:8000`) identifying *some other* `native-server`
  instance to pull from. This is **never persisted server-side** - no new
  field on `ServerOptions`, no new CLI flag, nothing in `cli_config.cpp`.
  It's supplied per-request by the web app as a query parameter, because the
  same running server might reasonably be pointed at different sources on
  different occasions (a dev Mac today, a different Mac tomorrow), and
  because there is exactly one device (the CLI Mac) that is ever a
  meaningful *source*, so there is nothing to configure ahead of time on the
  *pulling* side - only to type in, in the moment, in the GUI. The web app
  itself may remember the last-typed address client-side for convenience
  (see "Frontend" below) - that's a UI nicety, not part of the server's
  contract.

## Two roles, one library, one set of routes

Every `native-server` instance can act in either role; nothing at the route
level distinguishes CLI from app builds beyond `sharedDataDir` being empty
on non-CLI builds (same "no build-time branching" principle as the shipped
version).

- **Serving**: answering another device's questions about *this* device's
  own `shared_data` - "what packs do you have," "what files are in this
  pack," "give me this file's bytes." Only ever produces real answers when
  `sharedDataDir` is non-empty (in practice, only the CLI). No network calls
  *out* - purely reads its own local filesystem and responds.
- **Pulling**: asking some *other* device those same questions, then writing
  what comes back into this device's own `server_data`. Every build can do
  this, including the CLI (useful for local testing, and conceivable if a
  future setup ever has two "source" Macs).

## New library: `native-server/data_packs/`

Same sibling-library shape as before (`storage_map/`/`server_data/`
convention: pure, testable logic; `native_server_core`'s route handlers stay
thin wrappers around it) - but `syncPack`'s signature changes, `countFiles`
is retired (nothing calls it once sync no longer needs a local recursive
count), and a new `listPackFiles` is added.

```cpp
namespace data_packs {

// One directory directly under sharedDataDir - unchanged from the shipped
// version. Backs both the plain GET /data-packs (serving-mode) and the
// proxying GET /data-packs?remote=... (pulling-mode, see below) - the same
// function either way, only the route layer decides whose sharedDataDir to
// read vs. who to ask over the network.
std::vector<std::string> listPacks(const std::filesystem::path& sharedDataDir);

// New. Recursively walks sharedDataDir/packName and returns every regular
// file's path relative to the pack root (e.g.
// "dictionaries/wiktionaryapi/en-word.json") - order not guaranteed to be
// stable across filesystem implementations, callers must not depend on it
// beyond "every file appears exactly once". Throws
// std::filesystem::filesystem_error if packName doesn't exist under
// sharedDataDir - mirrors syncPack's old not-found behavior, now surfaced
// through this function instead. Backs the new GET /data-packs/:name/files
// route - the thing a puller calls first to find out what to fetch.
std::vector<std::string> listPackFiles(const std::filesystem::path& sharedDataDir,
                                        const std::string& packName);

struct SyncProgress {
  std::string pack;
  std::size_t total = 0;
  std::size_t copied = 0;
  bool done = true;
  std::optional<std::string> error;
};

// Pulls a pack over the network and writes it into destDir, preserving
// relative paths. Takes the actual network calls as two injected functions
// rather than an address - this is the key design choice that keeps
// data_packs itself free of any HTTP/https_client dependency, so its own
// tests can pass in trivial in-memory fakes instead of spinning up real
// servers (matching this project's "test the logic, not the network"
// convention, same reasoning as dictionary_utils splitting "parsing" tests
// from "live" ones):
//
//   listFiles   - returns every relative file path this pack has at the
//                 remote (what GET /data-packs/:name/files would return);
//                 expected to throw on any failure (network error, remote
//                 says 404 for an unknown pack, etc.).
//   fetchBytes  - given one relative path, returns that file's raw bytes
//                 (what a GET against the remote's /shared_data/<pack>/<path>
//                 static mount would return); expected to throw on failure
//                 the same way.
//
// The real implementations of both - constructed in data_packs_route.cpp,
// using https_client::HttpClient against http://<remote>/... URLs - are
// the only place in this whole feature that touches the network at all.
// syncPack calls listFiles() once up front (that count becomes `total`),
// then fetchBytes() once per file, create_directories-ing each file's
// parent as needed and overwriting anything already at destPath - same
// overwrite-in-place behavior, and same "accepted risk: no atomic writes"
// reasoning (see below), as the shipped version. Calls
// onProgress(copiedSoFar) after every file, from whatever thread this
// itself runs on - the caller (the route layer) still owns running this on
// a background thread and publishing progress somewhere pollable. Throws
// (propagating whatever listFiles/fetchBytes threw, or a std::filesystem
// error from the local write side) on the first failure rather than
// skipping and continuing - same "leave it exactly as far as it got, error
// visible, just re-sync from scratch afterward" behavior as before.
void syncPack(const std::function<std::vector<std::string>()>& listFiles,
              const std::function<std::string(const std::string& relativePath)>& fetchBytes,
              const std::filesystem::path& destDir,
              const std::function<void(std::size_t copiedSoFar)>& onProgress);

// Unchanged from the shipped version.
void clearServerData(const std::filesystem::path& destDir);
std::uintmax_t serverDataSizeBytes(const std::filesystem::path& destDir);

}  // namespace data_packs
```

**Retired**: `countFiles`. It only ever existed to size `SyncProgress::total`
before a local copy started; the network version gets `total` for free as
`listFiles().size()`, computed inside `syncPack` itself, so nothing else
needs a standalone file-counting utility. Deleted outright, not deprecated -
nothing else in the codebase calls it.

Every function above remains plain `std::filesystem`-based logic (or, for
`syncPack`, logic plus two caller-supplied closures it never inspects the
internals of) - still fully unit-testable with temp directories and fake
in-memory closures, no real HTTP or threading involved, no new dependency
added to this library's own `CMakeLists.txt`.

### Accepted risk: no atomic writes during sync

Unchanged from the shipped version: `syncPack` writes each file directly
rather than through `ServerDataStore::write()`'s temp-file-then-rename, so a
request landing on this exact machine at the exact moment a specific file is
mid-write could theoretically read a partial file. Accepted for the same
reasons as before (personal, low-traffic tool; infrequent, short sync runs;
the fix costs real overhead for a large pack's worth of files against a risk
this narrow) - now with the added latency of a network fetch per file on top,
which makes the window merely somewhat wider, not different in kind.

## New/changed HTTP routes (`native_server_core/lib/data_packs_route.cpp`)

Still no auth, same reasoning as before (`ServerOptions.host` already
defaults to `0.0.0.0`, nothing in this codebase's HTTP layer has
authentication) - and now doubly so, since restricting these routes to
`127.0.0.1` would break the exact cross-device use case they exist for.

**New dependency**: `native_server_core` (specifically `data_packs_route.cpp`)
now links `https_client`, to make the actual outbound calls described below.
Nothing else in this feature touches it - `data_packs` itself stays
dependency-free besides `std`.

- **`GET /data-packs`** - now has two modes, both reusing the exact same
  route:
  - No `?remote=` param (**serving** mode, unchanged from shipped): returns
    `{"packs": [...]}` from *this* device's own `sharedDataDir`, or
    `{"packs": []}` if it's empty/unset. This is what a puller's outbound
    call actually hits.
  - `?remote=<host:port>` present (**pulling** mode, new): this device
    makes its own outbound `GET http://<remote>/data-packs` (no param, i.e.
    the serving-mode call above, against the *other* device) via
    `https_client::HttpClient`, and relays that response straight back to
    the caller. Lets the web app ask "what packs does that address have"
    before committing to a sync - `GET /data-packs?remote=192.168.1.5:8000`
    from the iPhone webapp, against the iPhone's own local server. Returns
    `502 {"error": "..."}` if the remote is unreachable or returns something
    unexpected (matching `dictionary_route.cpp`'s own convention for
    upstream-fetch failures).
- **`GET /data-packs/:name/files`** (new, serving-only - no `?remote=` mode,
  nothing ever needs to proxy this one) - `{"files": ["dictionaries/...",
  ...]}`, backed directly by `listPackFiles`. `404 {"error": "..."}` for a
  pack name that doesn't exist under this device's own `sharedDataDir`. This
  is purely a server-to-server implementation detail - the web app itself
  never calls it directly, only a remote `native-server` does, from inside
  its own `syncPack` call.
- **New static mount: `/shared_data`** → `sharedDataDir`, registered in
  `server.cpp` right next to the existing `svr.set_mount_point("/server_data",
  serverDataDir.string())` line. Serves a pack's raw file bytes by relative
  path - `GET /shared_data/my-pack/dictionaries/wiktionaryapi/en-word.json` -
  exactly the shape `fetchBytes` needs to hit. No listing capability of its
  own (a static mount never offers directory listing) - that's exactly why
  `GET /data-packs/:name/files` exists as a separate, explicit endpoint.
- **`POST /data-packs/:name/sync`** - **now requires `?remote=<host:port>`**;
  `400 {"error": "..."}` immediately (synchronously, no network involved) if
  it's missing. There is no more param-less "local copy" mode at all - every
  sync is a pull from the given address, even when that address is this same
  device's own. Behavior once the param is present:
  - `409` if a sync/clear is already running (unchanged).
  - Otherwise: **`200 {"started": true}` immediately**, then a detached
    background thread (capturing `remote`, `serverDataDir`, `name` all by
    value, same by-value-capture convention as the shipped version) that:
    1. Constructs the real `listFiles`/`fetchBytes` closures against
       `http://<remote>/...` via a fresh `https_client::HttpClient`.
    2. Calls `data_packs::syncPack(...)`.
    3. On any exception - network unreachable, remote 404 for an unknown
       pack, a real fetch failure partway through, a local filesystem error
       - catches it and records `e.what()` into `gProgress.error`, same as
       today.

    **This is a real behavior change worth calling out explicitly**: "pack
    doesn't exist" used to be an immediate `404` response to the `POST`
    itself (checked synchronously against the local `sharedDataDir` before
    starting anything). Now, checking whether the pack exists means asking
    the *remote* - a network call - so that check moves inside the
    background thread alongside every other failure mode, and surfaces via
    `sync-status`'s `error` field once polled, not as an immediate HTTP
    status. The frontend's existing "immediate 404 → toast" branch in
    `startSync()` becomes dead code for the unknown-pack case specifically
    (it still fires for the synchronous missing-`?remote=` `400` case) - see
    "Frontend" below.
- **`GET /data-packs/sync-status`**, **`POST /data-packs/clear`**,
  **`GET /data-packs/server-data-size`** - **completely unchanged**. All
  three are always about *this* device's own state (the last/current sync's
  progress, this device's own `server_data`) regardless of where a sync's
  source was - exactly as before.

No "test connection" button/endpoint - a failed sync (bad address, remote
not running, wrong port) surfaces the same way any other sync failure does:
an error string in `sync-status`, shown via the existing toast system. One
fewer thing to build and keep in sync with the real failure paths.

## Frontend

`src/screens/DataPacksScreen.vue` gains a **remote address field** - a plain
text input (placeholder like `192.168.1.5:8000`) sitting above the pack
list. Proposed behavior (this specific piece wasn't pinned down as precisely
as everything above during design - flagging it here explicitly rather than
guessing silently):

- Typing an address and a **Load** action (on blur, or a small button -
  leaning toward blur-triggered since there's no separate "test connection"
  step to hang a button off of) calls `GET /data-packs?remote=<address>`,
  replacing the pack list with that address's packs, or a toast error if it
  fails (404/502/network error alike).
- The last-used address is remembered client-side via `StorageMap` (same
  mechanism `syncConfig.js` already uses for the Cloudflare Worker URL
  field on `SettingsScreen.vue`), pre-filled on mount so it isn't retyped
  every visit - purely a convenience, not part of the server's contract.
- Every **Sync** button's `POST` now includes `?remote=<address>` - the
  address that produced the currently-displayed pack list, so there's never
  a way to sync against a different address than the one the list came from.
- The dead "immediate 404 → toast" branch in `handleSync` goes away (see
  above); the missing-`?remote=` `400` case can't happen from this screen at
  all (the field is required before any pack list, and therefore any Sync
  button, can appear).

Everything else about the screen is unchanged from the shipped version:
per-pack progress bars, ~250ms polling of `sync-status`, the single **Clear**
button behind a plain `confirm()`, the `server_data: <size>` label refreshed
on mount and after sync/clear finish, on-mount restoration of an
already-in-progress sync.

`src/engine/dataPacksClient.js` signatures change to thread the address
through:

- `listPacks(remoteAddress)` - now always requires an address (no more
  param-less local listing); builds `/data-packs?remote=<address>`.
- `startSync(name, remoteAddress)` - builds
  `/data-packs/<name>/sync?remote=<address>`.
- `getSyncStatus()`, `getServerDataSize()`, `clearServerData()` - unchanged
  signatures, same endpoints.

## Testing plan

**`native-server/tests/unit/test_data_packs.cpp`** - substantially rewritten,
not preserved as-is, since it currently tests the retired local-copy
`syncPack` directly:

- `listPacks` - unchanged test cases (empty dir, several packs, ignores
  files-not-directories).
- `listPackFiles` (new) - returns every relative file path recursively,
  throws on an unknown pack name.
- `syncPack` (new shape) - inject fake `listFiles`/`fetchBytes` lambdas
  backed by an in-memory `std::map<std::string, std::string>` (no real
  network, no real remote filesystem): copies every file preserving relative
  structure; overwrites an existing destination file; creates the
  destination tree from scratch on a first-ever sync; calls `onProgress`
  once per file, strictly increasing, ending at the total; a `fetchBytes`
  that throws mid-loop stops there, propagating the exception, with
  whatever was written so far left in place.
- `clearServerData`, `serverDataSizeBytes` - unchanged.
- `countFiles`'s test cases are deleted along with the function itself.

**`native-server/tests/integration/test_data_packs_route.cpp`** -
substantially rewritten. The core new pattern this needs, novel for this
codebase: **two real `Server` instances in one test** - one acting as the
source (a real `sharedDataDir` with real pack files, reachable on
`127.0.0.1:<portA>`), one acting as the puller (empty `sharedDataDir`, real
`dataDir`, on `127.0.0.1:<portB>`) - with the puller's
`POST /data-packs/:name/sync?remote=127.0.0.1:<portA>` exercised against the
source over genuine loopback HTTP, then asserting the files really landed in
the puller's own `server_data`. Concretely:

- `GET /data-packs` (no param) - unchanged (empty `sharedDataDir` → `[]`;
  real pack dirs → sorted names).
- `GET /data-packs?remote=<address>` - proxies to a second real `Server`
  instance's own `/data-packs`; `502` when `remote` points at nothing
  listening.
- `GET /data-packs/:name/files` - real pack → sorted-or-just-present relative
  paths; unknown pack → `404`.
- `POST /data-packs/:name/sync` missing `?remote=` → `400`, synchronously, no
  thread spawned.
- `POST /data-packs/:name/sync?remote=<other real server's address>` - the
  two-server case above: files genuinely move from source's `shared_data`
  into puller's `server_data`, `sync-status` reflects real progress and a
  final `done: true, error: null`.
- `POST /data-packs/:name/sync?remote=<address with nothing listening>` -
  `200 {"started": true}` still returned immediately (per the new "always
  async" behavior above), but `sync-status` eventually reports `done: true`
  with a non-null `error` - this is the test that specifically locks in the
  behavior change from the old immediate-`404` (now: pack-not-found and
  remote-unreachable both surface identically, asynchronously, through
  `sync-status`).
- `POST /data-packs/:name/sync` while one's already running → `409`
  (unchanged mechanism, still worth a two-server test to be sure the
  network-based version doesn't accidentally lose the mutex gate).
- `POST /data-packs/clear`, `GET /data-packs/sync-status` (before any sync),
  `GET /data-packs/server-data-size` (before/after) - all unchanged.
- A same-machine self-targeting case (one `Server`, `remote` set to its own
  `127.0.0.1:<port>`, and its `sharedDataDir` pointed at real pack files) -
  confirms the "even self-targeting always goes over real HTTP" principle
  actually works, not just in theory.

`native-server/tests/support/server_tests_helper.h`'s `startTestServer()`
needs no signature change beyond what already exists (`sharedDataDir` is
already a parameter) - tests needing two servers just call it twice, on two
different ports.

## File map

- `native-server/data_packs/include/data_packs/data_packs.h`,
  `native-server/data_packs/lib/data_packs.cpp` - reworked `syncPack`,
  new `listPackFiles`, retired `countFiles`
- `native-server/native_server_core/lib/data_packs_route.h`/`.cpp` -
  reworked sync handler (async pack-existence check, `?remote=` required),
  `?remote=` proxying added to `GET /data-packs`, new
  `GET /data-packs/:name/files` handler, new `https_client` usage
- `native-server/native_server_core/lib/server.cpp` - new
  `/shared_data` static mount
- `native-server/native_server_core/CMakeLists.txt` - new `https_client`
  link dependency
- `native-server/tests/unit/test_data_packs.cpp`,
  `native-server/tests/integration/test_data_packs_route.cpp` - rewritten
  per "Testing plan" above
- `src/screens/DataPacksScreen.vue` - new remote-address field
- `src/engine/dataPacksClient.js` - `listPacks`/`startSync` gain a
  `remoteAddress` parameter
- Unchanged: `native-server/native_server_core/include/native_server/server.h`
  (`ServerOptions::sharedDataDir`), `native_server_cli_lib` config parsing,
  `native_server_cli/main.cpp`, `src/screens/SettingsScreen.vue`'s link to
  this screen, `src/App.vue`'s wiring
