/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/icelogger.h"

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/rtc_event_log.h"

namespace webrtc {

IceEventLog::IceEventLog() {}
IceEventLog::~IceEventLog() {}

// RTC event logs aren't working properly with ICEEventLogs. Not logging these
// to get proper RTC event logs for a local run.
void IceEventLog::LogCandidatePairConfig(
    IceCandidatePairConfigType type,
    uint32_t candidate_pair_id,
    const IceCandidatePairDescription& candidate_pair_desc) {
  return;
}

void IceEventLog::LogCandidatePairEvent(IceCandidatePairEventType type,
                                        uint32_t candidate_pair_id) {
  return;
}

void IceEventLog::DumpCandidatePairDescriptionToMemoryAsConfigEvents() const {
  return;
}

}  // namespace webrtc
