/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_ICE_TRANSPORT_H_
#define PC_ICE_TRANSPORT_H_

#include "api/ice_transport_interface.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Implementation of IceTransportInterface that does not take ownership
// of its underlying IceTransport. It depends on its creator class to
// ensure that Close() is called before the underlying IceTransport
// is deallocated.
class IceTransportWithPointer : public IceTransportInterface {
 public:
  explicit IceTransportWithPointer(cricket::IceTransportInternal* internal)
      : signaling_thread_(rtc::Thread::Current()), internal_(internal) {}

  cricket::IceTransportInternal* internal() override;

  void Clear();

 protected:
  ~IceTransportWithPointer() override;

 private:
  rtc::Thread* signaling_thread_;
  cricket::IceTransportInternal* internal_;
};

}  // namespace webrtc

#endif  // PC_ICE_TRANSPORT_H_
