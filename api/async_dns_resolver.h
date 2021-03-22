/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ASYNC_DNS_RESOLVER_H_
#define API_ASYNC_DNS_RESOLVER_H_

#include <memory>

#include "rtc_base/socket_address.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// This interface defines the methods to resolve a hostname asynchronously.
// The AsyncDnsResolverInterface class encapsulates a single name query.
//
// Usage:
//   std::unique_ptr<AsyncDnsResolverInterface> resolver =
//        factory->Create(address-to-be-resolved, [resolver]() {
//     if (resolver->GetResolvedAddress(AF_INET, &addr) {
//       // success
//     } else {
//       // failure
//       error = resolver->GetError();
//     }
//     resolver->Stop();
//     resolver.clear();
//   });

class RTC_EXPORT AsyncDnsResolverInterface {
 public:
  virtual ~AsyncDnsResolverInterface() = default;

  // Start address resolution of the hostname in |addr|.
  virtual void Start(const rtc::SocketAddress& addr,
                     std::function<void()> callback) = 0;
  // Returns true iff the address from |Start| was successfully resolved.
  // If the address was successfully resolved, sets |addr| to a copy of the
  // address from |Start| with the IP address set to the top most resolved
  // address of |family| (|addr| will have both hostname and the resolved ip).
  virtual bool GetResolvedAddress(int family,
                                  rtc::SocketAddress* addr) const = 0;
  // Returns error from resolver.
  virtual int GetError() const = 0;
  // Releases any resources held by resolver. Must be called before deleting.
  virtual void Stop() = 0;
};

// An abstract factory for creating AsyncDnsResolverInterfaces. This allows
// client applications to provide WebRTC with their own mechanism for
// performing DNS resolution.
class AsyncDnsResolverFactoryInterface {
 public:
  virtual ~AsyncDnsResolverFactoryInterface() = default;

  virtual std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndStart(
      const rtc::SocketAddress& addr,
      std::function<void()> callback) = 0;
  // For backwards compatibility: Requires a separate Start() call on the
  // resolver.
  virtual std::unique_ptr<webrtc::AsyncDnsResolverInterface> Create() = 0;
};

}  // namespace webrtc

#endif  // API_ASYNC_DNS_RESOLVER_H_
