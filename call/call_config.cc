/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/call_config.h"

#include "rtc_base/checks.h"

namespace webrtc {

CallConfig::CallConfig(const Context& context,
                       TaskQueueBase* network_task_queue)
    : context(context), network_task_queue_(network_task_queue) {}

RtpTransportConfig CallConfig::ExtractTransportConfig() const {
  RtpTransportConfig transportConfig = {.context = context};
  transportConfig.bitrate_config = bitrate_config;
  transportConfig.network_controller_factory = network_controller_factory;
  transportConfig.network_state_predictor_factory =
      network_state_predictor_factory;
  transportConfig.pacer_burst_interval = pacer_burst_interval;
  return transportConfig;
}


}  // namespace webrtc
