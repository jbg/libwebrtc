/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONFIG_H_
#define CALL_RTP_TRANSPORT_CONFIG_H_

#include <memory>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/network_state_predictor.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/network_control.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

struct RtpTransportConfig {
  Environment env;

  // Bitrate config used until valid bitrate estimates are calculated. Also
  // used to cap total bitrate used. This comes from the remote connection.
  BitrateConstraints bitrate_config;

  // NetworkStatePredictor to use for this call.
  NetworkStatePredictorFactoryInterface* network_state_predictor_factory =
      nullptr;

  // Network controller factory to use for this call.
  NetworkControllerFactoryInterface* network_controller_factory = nullptr;

  // The burst interval of the pacer, see TaskQueuePacedSender constructor.
  absl::optional<TimeDelta> pacer_burst_interval;
};
}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_CONFIG_H_
