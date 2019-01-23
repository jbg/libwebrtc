/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/ice_transport_factory.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port_allocator.h"
#include "rtc_base/thread.h"

namespace webrtc {

namespace {
// This implementation of IceTransportInterface is used in cases where
// the only reference to the P2PTransport will be through this class.
class IceTransportWithTransportChannel : public IceTransportInterface {
 public:
  IceTransportWithTransportChannel(
      std::unique_ptr<cricket::IceTransportInternal> internal)
      : signaling_thread_(rtc::Thread::Current()),
        internal_(std::move(internal)) {}

  cricket::IceTransportInternal* internal() override {
    RTC_DCHECK(signaling_thread_->IsCurrent());
    return internal_.get();
  }

 private:
  rtc::Thread* signaling_thread_;
  std::unique_ptr<cricket::IceTransportInternal> internal_;
};

}  // namespace

rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
    cricket::PortAllocator* port_allocator) {
  return new rtc::RefCountedObject<IceTransportWithTransportChannel>(
      absl::make_unique<cricket::P2PTransportChannel>("", 0, port_allocator));
}

}  // namespace webrtc
