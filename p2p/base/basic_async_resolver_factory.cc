/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basic_async_resolver_factory.h"

#include <functional>
#include <memory>

#include "absl/memory/memory.h"
#include "api/async_dns_resolver.h"
#include "rtc_base/async_resolver.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace webrtc {

rtc::AsyncResolverInterface* BasicAsyncResolverFactory::Create() {
  return new rtc::AsyncResolver();
}

class WrappingAsyncDnsResolver : public AsyncDnsResolverInterface,
                                 public sigslot::has_slots<> {
 public:
  explicit WrappingAsyncDnsResolver(rtc::AsyncResolverInterface* wrapped)
      : wrapped_(absl::WrapUnique(wrapped)) {}

  ~WrappingAsyncDnsResolver() override { RTC_DCHECK(stopped_); }

  void Start(const rtc::SocketAddress& addr,
             std::function<void()> callback) override {
    wrapped_->SignalDone.connect(this,
                                 &WrappingAsyncDnsResolver::OnResolveResult);
    wrapped_->Start(addr);
  }
  bool GetResolvedAddress(int family, rtc::SocketAddress* addr) const override {
    return wrapped_->GetResolvedAddress(family, addr);
  }
  int GetError() const override { return wrapped_->GetError(); }
  void Stop() override {
    stopped_ = true;
    wrapped_.release()->Destroy(false);
  }

 private:
  void OnResolveResult(rtc::AsyncResolverInterface* ref) {
    RTC_DCHECK(ref == wrapped_.get());
    callback_();
  }

  std::function<void()> callback_;
  std::unique_ptr<rtc::AsyncResolverInterface> wrapped_;
  bool stopped_ = false;
};

std::unique_ptr<webrtc::AsyncDnsResolverInterface>
WrappingAsyncDnsResolverFactory::Create() {
  return std::make_unique<WrappingAsyncDnsResolver>(wrapped_factory_->Create());
}

}  // namespace webrtc
