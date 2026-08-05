#include <https_client/http_client.h>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using https_client::HeaderList;
using https_client::HttpClient;
using https_client::HttpError;

TEST_CASE("GET returns real content from a live HTTPS server", "[https_client]") {
  HttpClient client;
  auto res = client.get("https://postman-echo.com/get?foo=bar", {});

  REQUIRE(res.statusCode == 200);
  auto body = nlohmann::json::parse(res.body);
  REQUIRE(body.at("args").at("foo") == "bar");
}

TEST_CASE("GET sends custom request headers", "[https_client]") {
  HttpClient client;
  auto res = client.get("https://postman-echo.com/get",
                         HeaderList{{"X-Https-Client-Test", "hello"}});

  REQUIRE(res.statusCode == 200);
  auto body = nlohmann::json::parse(res.body);
  REQUIRE(body.at("headers").at("x-https-client-test") == "hello");
}

TEST_CASE("POST sends a body", "[https_client]") {
  HttpClient client;
  auto res = client.post("https://postman-echo.com/post",
                          HeaderList{{"Content-Type", "application/json"}},
                          R"({"hello":"world"})");

  REQUIRE(res.statusCode == 200);
  auto body = nlohmann::json::parse(res.body);
  REQUIRE(body.at("json").at("hello") == "world");
}

TEST_CASE("a non-2xx status is returned normally, not thrown", "[https_client]") {
  HttpClient client;
  auto res = client.get("https://postman-echo.com/status/404", {});

  REQUIRE(res.statusCode == 404);
}

TEST_CASE("an unreachable host throws HttpError", "[https_client]") {
  HttpClient client;
  REQUIRE_THROWS_AS(client.get("https://definitely-does-not-exist.invalid/", {}), HttpError);
}

// Not testing youtube_utils/transcript logic here (that's a separate
// library) - just confirming https_client itself actually works against
// the real target server it exists for, TLS chain and all, before
// youtube_utils gets built on top of it.
TEST_CASE("GET works against a real youtube.com endpoint", "[https_client]") {
  HttpClient client;
  auto res = client.get("https://www.youtube.com/robots.txt", {});

  REQUIRE(res.statusCode == 200);
  REQUIRE(res.body.find("User-agent") != std::string::npos);
}
