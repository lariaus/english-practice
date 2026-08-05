//
//  english_practice_appTests.swift
//  english-practice-appTests
//
//  Created by Steven on 02.08.2026.
//

import Foundation
import Testing

private final class BundleToken {}

struct english_practice_appTests {

    @Test func embeddedServerServesFixtureContent() async throws {
        let indexURL = try #require(Bundle(for: BundleToken.self).url(forResource: "index", withExtension: "html"))
        let rootDir = indexURL.deletingLastPathComponent().path

        let dataDir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: dataDir) }

        let port: Int32 = 18790
        let handle = try #require(native_server_create("127.0.0.1", port, rootDir, dataDir.path))
        defer { native_server_destroy(handle) }

        #expect(native_server_start(handle) == 0)
        defer { native_server_stop(handle) }

        let url = URL(string: "http://127.0.0.1:\(port)/index.html")!
        let (data, _) = try await URLSession.shared.data(from: url)
        let body = String(data: data, encoding: .utf8)

        #expect(body == "xctest fixture\n")
    }

}
