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

// Implementation of IceTransportWithPointer
IceTransportWithPointer::~IceTransportWithPointer() {
  // We depend on the signaling thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(signaling_thread_->IsCurrent() || !internal_);
}

cricket::IceTransportInternal* IceTransportWithPointer::internal() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return internal_;
}

void IceTransportWithPointer::Clear() {
  internal_ = nullptr;
}

}  // namespace webrtc
