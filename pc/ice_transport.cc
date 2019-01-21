/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/ice_transport.h"

#include <memory>
#include <utility>

namespace webrtc {

// This implementation of IceTransportInterface is used in cases where
// the only reference to the P2PTransport will be through this class.
class IceTransportWithTransportChannel : public IceTransportInterface {
 public:
  IceTransportWithTransportChannel(
      std::unique_ptr<cricket::P2PTransportChannel> internal);

  cricket::IceTransportInternal* internal() override { return internal_.get(); }

 private:
  std::unique_ptr<cricket::IceTransportInternal> internal_;
};

IceTransportWithTransportChannel::IceTransportWithTransportChannel(
    std::unique_ptr<cricket::P2PTransportChannel> internal) {
  internal_ = std::move(internal);
}

rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
    cricket::PortAllocator* port_allocator) {
  return new rtc::RefCountedObject<IceTransportWithTransportChannel>(
      absl::WrapUnique(
          new cricket::P2PTransportChannel("", 0, port_allocator)));
}

// Implementation of IceTransportWithPointer
void IceTransportWithPointer::Clear() {
  internal_ = nullptr;
}

}  // namespace webrtc
