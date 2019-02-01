/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_CLOCK_OFFSET_CALCULATOR_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_CLOCK_OFFSET_CALCULATOR_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "rtc_tools/event_log_visualizer/ice_transaction.h"

namespace webrtc {

// Estimates the offset between the clocks in two RTCEventLogs using STUN
// transactions recorded in the logs' LoggedIceCandidatePairEvents.
// TODO(zstein): Look for other correlated event types that could be used.
// The clocks can start at an arbitrary timestamp.
// Assumes network delay is symmetric.
// Does not account for clock drift.
class ClockOffsetCalculator final {
 public:
  ClockOffsetCalculator();
  ~ClockOffsetCalculator();

  void ProcessTransactions(const webrtc::IceTransactions& transactions);

  int64_t CalculateMeanOffsetMs() const;
  int64_t CalculateMedianOffsetMs();
  // The number of full sequences that will be used to calculate averages.
  int64_t FullSequenceCount() const;

 private:
  absl::optional<int64_t> CalculateOffsetMs(
      const std::vector<absl::optional<uint64_t>>& sequence);

  std::vector<int64_t> offsets_ms_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_CLOCK_OFFSET_CALCULATOR_H_
