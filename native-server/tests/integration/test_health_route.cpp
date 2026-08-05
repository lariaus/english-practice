#include "support/server_tests_helper.h"

#include <httplib.h>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using native_server_test::kTestHost;
using native_server_test::startTestServer;

namespace {
constexpr uint16_t kTestPort = 18082;
}

TEST_CASE("GET /health returns 200 with a status field", "[integration][health]") {
  auto server = startTestServer(kTestPort);
  httplib::Client client(kTestHost, kTestPort);

  auto res = client.Get("/health");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = nlohmann::json::parse(res->body);
  REQUIRE(body.at("status") == "ok");
}
