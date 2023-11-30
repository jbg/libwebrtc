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

CallConfig::CallConfig(const Environment& env,
                       TaskQueueBase* network_task_queue)
    : env(env), network_task_queue_(network_task_queue) {}

CallConfig::CallConfig(const CallConfig& config) = default;

CallConfig::~CallConfig() = default;

RtpTransportConfig CallConfig::ExtractTransportConfig() const {
  RtpTransportConfig transportConfig = {.env = env};
  transportConfig.bitrate_config = bitrate_config;
  transportConfig.network_controller_factory = network_controller_factory;
  transportConfig.network_state_predictor_factory =
      network_state_predictor_factory;
  return transportConfig;
}


}  // namespace webrtc
