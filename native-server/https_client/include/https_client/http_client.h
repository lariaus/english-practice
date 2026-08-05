#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace https_client {

using HeaderList = std::vector<std::pair<std::string, std::string>>;

struct HttpResponse {
  int statusCode = 0;
  HeaderList headers;
  std::string body;
};

// Thrown for transport-level failures (DNS, connectivity, TLS, timeout) -
// never for a non-2xx HTTP status, which is returned as a normal
// HttpResponse instead (matching how NSURLSession itself distinguishes
// the two).
class HttpError : public std::runtime_error {
 public:
  explicit HttpError(const std::string& message) : std::runtime_error(message) {}
};

// Wraps NSURLSession behind a synchronous, pure-C++ interface. Apple-only.
class HttpClient {
 public:
  explicit HttpClient(double timeoutSeconds = 15.0);
  ~HttpClient();

  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  HttpResponse get(const std::string& url, const HeaderList& headers);
  HttpResponse post(const std::string& url, const HeaderList& headers, const std::string& body);

 private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

}  // namespace https_client
