#include <https_client/http_client.h>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using https_client::HeaderList;
using https_client::HttpClient;

// Plain http:// tests. https_client is named/built for HTTPS (its only
// real target is https://www.youtube.com), but NSURLSession doesn't
// distinguish schemes at the API level, so it's worth confirming what
// actually happens on a plain HTTP request rather than leaving it
// unknown - see test_https_client.cpp for the HTTPS-specific tests.

TEST_CASE("GET works over plain HTTP", "[http_client]") {
  HttpClient client;
  auto res = client.get("http://postman-echo.com/get?foo=bar", {});

  REQUIRE(res.statusCode == 200);
  auto body = nlohmann::json::parse(res.body);
  REQUIRE(body.at("args").at("foo") == "bar");
}

TEST_CASE("POST works over plain HTTP", "[http_client]") {
  HttpClient client;
  auto res = client.post("http://postman-echo.com/post",
                          HeaderList{{"Content-Type", "application/json"}},
                          R"({"hello":"world"})");

  REQUIRE(res.statusCode == 200);
  auto body = nlohmann::json::parse(res.body);
  REQUIRE(body.at("json").at("hello") == "world");
}
