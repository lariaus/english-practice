#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void* NativeServerHandle;

// dataDir backs /storage/maps/... - unlike rootDir, doesn't need to
// already exist (created on first write). Returns NULL on failure (bad
// rootDir, etc.) - never throws across the boundary.
NativeServerHandle native_server_create(const char* host, int port, const char* rootDir,
                                         const char* dataDir);

// Returns 0 on success, non-zero on failure (e.g. bind failed) - never
// throws across the boundary.
int native_server_start(NativeServerHandle handle);

void native_server_stop(NativeServerHandle handle);
void native_server_destroy(NativeServerHandle handle);

#ifdef __cplusplus
}
#endif
