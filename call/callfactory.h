/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_CALLFACTORY_H_
#define CALL_CALLFACTORY_H_

#include <memory>

#include "api/call/callfactoryinterface.h"
#include "call/rtp_transport_controller_send_interface.h"

namespace webrtc {

class CallFactory : public CallFactoryInterface {
  ~CallFactory() override {}

  Call* CreateCall(const CallConfig& config) override;
  Call* CreateCall(const CallConfig& config,
                   std::unique_ptr<RtpTransportControllerSendInterface>
                       transport_send) override;
};

}  // namespace webrtc

#endif  // CALL_CALLFACTORY_H_
