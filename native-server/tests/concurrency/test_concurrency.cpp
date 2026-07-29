#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

using native_server_test::kTestHost;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18081;
}

TEST_CASE("handles more concurrent requests than the thread pool size", "[concurrency]") {
  auto server = startTestServer(kTestPort);

  constexpr int kRequestCount = 40;
  std::vector<std::thread> threads;
  std::vector<int> statuses(kRequestCount, 0);
  std::vector<httplib::Error> errors(kRequestCount, httplib::Error::Success);

  for (int i = 0; i < kRequestCount; ++i) {
    threads.emplace_back([&, i] {
      httplib::Client client(kTestHost, kTestPort);
      auto res = client.Get("/index.html");
      statuses[i] = res ? res->status : -1;
      errors[i] = res.error();
    });
  }
  for (auto& t : threads) t.join();

  for (int i = 0; i < kRequestCount; ++i) {
    INFO("request " << i << " error: " << httplib::to_string(errors[i]));
    REQUIRE(statuses[i] == 200);
  }
}
