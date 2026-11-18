#pragma once

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumNativeTests/SimpleAssetRequest.h>
#include <CesiumNativeTests/SimpleAssetResponse.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace mayhem {

// Runs every task inline so that `Future::wait()` completes on the calling thread.
class InlineTaskProcessor : public CesiumAsync::ITaskProcessor {
public:
  void startTask(std::function<void()> f) override { f(); }
};

// Answers every request with the same status and body, so no harness touches the network or disk.
// The default is an empty 404; a harness that fuzzes a server response passes the fuzz bytes as `body`.
class FixedResponseAccessor : public CesiumAsync::IAssetAccessor {
public:
  explicit FixedResponseAccessor(
      uint16_t statusCode = 404,
      std::vector<std::byte> body = {})
      : _statusCode(statusCode), _body(std::move(body)) {}

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
  get(const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& url,
      const std::vector<THeader>& headers) override {
    return request(asyncSystem, "GET", url, headers, {});
  }

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& verb,
      const std::string& url,
      const std::vector<THeader>& headers,
      const std::span<const std::byte>&) override {
    auto pResponse = std::make_unique<CesiumNativeTests::SimpleAssetResponse>(
        _statusCode,
        "text/plain",
        CesiumAsync::HttpHeaders{},
        _body);
    std::shared_ptr<CesiumAsync::IAssetRequest> pRequest =
        std::make_shared<CesiumNativeTests::SimpleAssetRequest>(
            verb,
            url,
            CesiumAsync::HttpHeaders(headers.begin(), headers.end()),
            std::move(pResponse));
    return asyncSystem.createResolvedFuture(std::move(pRequest));
  }

  void tick() noexcept override {}

private:
  uint16_t _statusCode;
  std::vector<std::byte> _body;
};

using NotFoundAccessor = FixedResponseAccessor;

inline std::span<const std::byte> asBytes(const uint8_t* data, size_t size) {
  return {reinterpret_cast<const std::byte*>(data), size};
}

} // namespace mayhem
