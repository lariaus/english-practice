//
//  ContentView.swift
//  english-practice-app
//
//  Created by Steven on 02.08.2026.
//

import SwiftUI

struct ContentView: View {
    var body: some View {
        WebView(url: URL(string: "http://127.0.0.1:\(nativeServerPort)/")!)
            .ignoresSafeArea()
    }
}

#Preview {
    ContentView()
}
