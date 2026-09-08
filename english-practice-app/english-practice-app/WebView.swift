import SwiftUI
import WebKit

struct WebView {
    let url: URL

    func makeCoordinator() -> Coordinator {
        Coordinator(url: url)
    }

    final class Coordinator: NSObject, WKNavigationDelegate, WKUIDelegate {
        let url: URL

        init(url: URL) {
            self.url = url
        }

        func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
            retry(webView)
        }

        func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
            retry(webView)
        }

        private func retry(_ webView: WKWebView) {
            // The embedded HTTP server starts on a background thread at app
            // launch; the first load can race it, so retry briefly.
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
                webView.load(URLRequest(url: self.url))
            }
        }

        func webView(
            _ webView: WKWebView,
            requestMediaCapturePermissionFor origin: WKSecurityOrigin,
            initiatedByFrame frame: WKFrameInfo,
            type: WKMediaCaptureType,
            decisionHandler: @escaping (WKPermissionDecision) -> Void
        ) {
            decisionHandler(.grant)
        }
    }
}

// The embedded webapp is rebuilt on every launch (see build-xcode.sh), but
// WKWebView keeps its own on-disk HTTP cache across launches/reinstalls -
// separate from (and unaffected by clearing) native-server's own
// Application-Support-backed storage (StorageMap, synced packs, etc.), so
// this never touches the Cloudflare Worker URL or anything else the user
// has configured. Only disk/memory cache is cleared - not cookies/
// localStorage/IndexedDB - and the load happens inside the completion
// handler so it can't race the clear and hit stale cache anyway.
private func loadFresh(_ webView: WKWebView, url: URL) {
    let cacheTypes: Set<String> = [WKWebsiteDataTypeDiskCache, WKWebsiteDataTypeMemoryCache]
    WKWebsiteDataStore.default().removeData(ofTypes: cacheTypes, modifiedSince: .distantPast) {
        webView.load(URLRequest(url: url))
    }
}

#if os(macOS)
extension WebView: NSViewRepresentable {
    func makeNSView(context: Context) -> WKWebView {
        let webView = WKWebView()
        webView.navigationDelegate = context.coordinator
        webView.uiDelegate = context.coordinator
        loadFresh(webView, url: url)
        return webView
    }

    func updateNSView(_ nsView: WKWebView, context: Context) {}
}
#else
extension WebView: UIViewRepresentable {
    func makeUIView(context: Context) -> WKWebView {
        let configuration = WKWebViewConfiguration()
        configuration.allowsInlineMediaPlayback = true
        let webView = WKWebView(frame: .zero, configuration: configuration)
        webView.navigationDelegate = context.coordinator
        webView.uiDelegate = context.coordinator
        loadFresh(webView, url: url)
        return webView
    }

    func updateUIView(_ uiView: WKWebView, context: Context) {}
}
#endif
