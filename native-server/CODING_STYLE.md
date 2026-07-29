# C++ naming conventions (native-server/)

- **Types** (classes, structs, enums): `PascalCase` - e.g. `Server`, `ServerOptions`.
- **Functions/methods**: `camelCase` - e.g. `startServer`, `isRunning`.
- **Variables** (locals, parameters): `camelCase` - e.g. `rootDir`, `envLookup`.
- **Public member variables**: `camelCase`, no prefix - e.g. `isPublic`.
- **Private member variables**: leading underscore + `camelCase` - e.g. `_isPrivate`.

Extensions decided while applying the above (not explicitly specified, kept
consistent with it):

- **Compile-time constants**: `k` prefix + `PascalCase` - e.g. `kThreadPoolSize`.
- **Namespaces**: lowercase `snake_case` - e.g. `native_server`,
  `native_server_cli` - standard C++ convention, orthogonal to the
  identifier-casing rules above.
- **File names**: lowercase `snake_case` - e.g. `server.h`, `cli_config.cpp` -
  matches the existing directory-naming convention already in place
  (`native_server_core/`, etc.).
- **Module-level file-local statics** (e.g. a signal handler's server
  pointer in `main.cpp`): `camelCase`, no prefix - they're not class
  members, so the public/private member rule doesn't apply to them.

## Project structure

One CMake target per directory, one `CMakeLists.txt` per target:

- **Library** target directories get both `include/` (public headers, under
  their namespace, e.g. `include/native_server/server.h`) and `lib/`
  (`.cpp` sources, e.g. `lib/server.cpp`).
- **Binary** target directories get neither - just the source file(s)
  directly inside (e.g. `native_server_cli/main.cpp`) - nothing links
  against a binary, so it has no public headers to expose.
- Directory names match their CMake target name exactly (e.g.
  `native_server_core/` builds the `native_server_core` target).
- The top-level `CMakeLists.txt` only does project-wide setup - `project()`,
  shared `FetchContent` declarations (httplib, nlohmann_json, Catch2), and
  `add_subdirectory()` for each target - it never defines a target itself.
