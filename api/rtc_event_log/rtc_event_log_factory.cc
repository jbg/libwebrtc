/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtc_event_log/rtc_event_log_factory.h"

#include <memory>
#include <utility>

#include "api/connection_environment.h"
#include "api/field_trials_view.h"
#include "rtc_base/checks.h"

#ifdef WEBRTC_ENABLE_RTC_EVENT_LOG
#include "logging/rtc_event_log/rtc_event_log_impl.h"
#endif

namespace webrtc {

std::unique_ptr<RtcEventLog> RtcEventLogFactory::Create(
    const ConnectionEnvironment& env) const {
#ifdef WEBRTC_ENABLE_RTC_EVENT_LOG
  if (env.experiments().IsEnabled("WebRTC-RtcEventLogKillSwitch")) {
    return std::make_unique<RtcEventLogNull>();
  }
  RtcEventLog::EncodingType encoding_type =
      env.experiments().IsDisabled("WebRTC-RtcEventLogNewFormat")
          ? RtcEventLog::EncodingType::Legacy
          : RtcEventLog::EncodingType::NewFormat;

  return std::make_unique<RtcEventLogImpl>(
      env, RtcEventLogImpl::CreateEncoder(encoding_type));
#else
  return std::make_unique<RtcEventLogNull>();
#endif
}

}  // namespace webrtc
