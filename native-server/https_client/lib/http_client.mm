#import <Foundation/Foundation.h>

#include "https_client/http_client.h"

#include <dispatch/dispatch.h>

namespace https_client {

struct HttpClient::Impl {
  NSURLSession* session;
};

namespace {

NSMutableURLRequest* buildRequest(const std::string& url, const HeaderList& headers,
                                   const std::string& method) {
  NSString* nsUrl = [NSString stringWithUTF8String:url.c_str()];
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:nsUrl]];
  request.HTTPMethod = [NSString stringWithUTF8String:method.c_str()];

  for (const auto& header : headers) {
    NSString* name = [NSString stringWithUTF8String:header.first.c_str()];
    NSString* value = [NSString stringWithUTF8String:header.second.c_str()];
    [request setValue:value forHTTPHeaderField:name];
  }

  return request;
}

HttpResponse runSynchronousDataTask(NSURLSession* session, NSURLRequest* request) {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

  __block NSInteger blockStatusCode = 0;
  __block NSDictionary<NSString*, NSString*>* blockHeaders = nil;
  __block NSData* blockData = nil;
  __block NSError* blockError = nil;

  NSURLSessionDataTask* task = [session
      dataTaskWithRequest:request
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
          blockError = error;
          if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]]) {
            NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
            blockStatusCode = httpResponse.statusCode;
            blockHeaders = httpResponse.allHeaderFields;
            blockData = data;
          }
          dispatch_semaphore_signal(semaphore);
        }];

  [task resume];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  if (blockError != nil) {
    std::string message = blockError.localizedDescription.UTF8String;
    throw HttpError("Network error: " + message);
  }

  HttpResponse result;
  result.statusCode = static_cast<int>(blockStatusCode);

  for (NSString* key in blockHeaders) {
    result.headers.emplace_back(key.UTF8String, blockHeaders[key].UTF8String);
  }

  if (blockData != nil) {
    result.body.assign(reinterpret_cast<const char*>(blockData.bytes), blockData.length);
  }

  return result;
}

}  // namespace

HttpClient::HttpClient(double timeoutSeconds) : _impl(new Impl) {
  NSURLSessionConfiguration* configuration =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  configuration.timeoutIntervalForRequest = timeoutSeconds;
  _impl->session = [NSURLSession sessionWithConfiguration:configuration];
}

HttpClient::~HttpClient() {
  [_impl->session finishTasksAndInvalidate];
}

HttpResponse HttpClient::get(const std::string& url, const HeaderList& headers) {
  @autoreleasepool {
    NSURLRequest* request = buildRequest(url, headers, "GET");
    return runSynchronousDataTask(_impl->session, request);
  }
}

HttpResponse HttpClient::post(const std::string& url, const HeaderList& headers,
                               const std::string& body) {
  @autoreleasepool {
    NSMutableURLRequest* request = buildRequest(url, headers, "POST");
    request.HTTPBody = [NSData dataWithBytes:body.data() length:body.size()];
    return runSynchronousDataTask(_impl->session, request);
  }
}

}  // namespace https_client
