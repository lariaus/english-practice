//
//  english_practice_appApp.swift
//  english-practice-app
//
//  Created by Steven on 02.08.2026.
//

import SwiftUI

let nativeServerPort: Int32 = 8765

private var nativeServerHandle: UnsafeMutableRawPointer?

@main
struct english_practice_appApp: App {
    init() {
        DispatchQueue.global(qos: .userInitiated).async {
            guard let resourcePath = Bundle.main.resourcePath else { return }
            let rootDir = resourcePath + "/dist"

            // Application Support - same API on macOS and iOS, sandboxed,
            // persists across launches, not created automatically (must
            // mkdir ourselves). Backs /storage/maps/... - see
            // docs/local-storage.md.
            guard let supportDir = FileManager.default.urls(
                for: .applicationSupportDirectory, in: .userDomainMask
            ).first else { return }
            let dataDir = supportDir.appendingPathComponent("NativeServerData")
            try? FileManager.default.createDirectory(at: dataDir, withIntermediateDirectories: true)

            guard let handle = native_server_create("127.0.0.1", nativeServerPort, rootDir, dataDir.path) else { return }
            if native_server_start(handle) == 0 {
                nativeServerHandle = handle
            } else {
                native_server_destroy(handle)
            }
        }
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
