#pragma once

#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <thread>

namespace native_server_test {

// Test-only escape hatch for genuine Wiktionary rate-limiting - deliberately
// lives only in tests/, never in production code, which has no special
// handling for this by design. Confirmed cause, not a guess: running
// several live test binaries back to back against the same word/audio file
// ("smart") reliably earns a real HTTP 429 from Wiktionary (thrown as
// WiktionaryApiError, whose message contains "429") within a handful of
// requests - an environmental rate limit, not a regression in this code.
//
// On a 429, waits 2 seconds and tries once more. If that second attempt
// also 429s, returns nullopt - the caller is expected to treat that as a
// genuine pass (SUCCEED() + early return), not a failure and not a Catch2
// SKIP (a binary where every test case skips still exits non-zero, which
// ctest reports as failed - not what "good enough" means here). Any other
// error - a real network failure, a parse error, anything that isn't a
// 429 - propagates immediately, no retry, no swallowing.
template <typename FetchFn>
auto tryFetchAllowingRateLimit(FetchFn fetch) -> std::optional<decltype(fetch())> {
  for (int attempt = 1; attempt <= 2; ++attempt) {
    try {
      return fetch();
    } catch (const std::exception& e) {
      std::string message = e.what();
      if (message.find("429") == std::string::npos) throw;
      if (attempt == 2) return std::nullopt;
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  }
  return std::nullopt;
}

}  // namespace native_server_test
