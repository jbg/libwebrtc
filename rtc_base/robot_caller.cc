/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/robot_caller.h"

namespace webrtc {
namespace robot_caller_impl {

RobotCallerReceivers::RobotCallerReceivers() = default;
RobotCallerReceivers::~RobotCallerReceivers() = default;

void RobotCallerReceivers::AddReceiverImpl(UntypedFunction* f) {
  receivers_.push_back(std::move(*f));
}

void RobotCallerReceivers::Foreach(
    rtc::FunctionView<void(UntypedFunction&)> fv) {
  for (auto& r : receivers_) {
    fv(r);
  }
}

}  // namespace robot_caller_impl
}  // namespace webrtc
