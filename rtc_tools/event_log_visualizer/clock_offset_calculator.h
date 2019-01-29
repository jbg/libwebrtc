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

#include "logging/rtc_event_log/rtc_event_log_parser.h"

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

  // The number of full sequences that will be used to calculate averages.
  int64_t full_sequence_count() const { return full_sequence_count_; }

  void ProcessLogs(const std::vector<LoggedIceCandidatePairEvent>& log1,
                   const std::vector<LoggedIceCandidatePairEvent>& log2);

  int64_t CalculateMean() const;
  int64_t CalculateMedian();

  // TODO(zstein): This is not useful as is - revisit.
  // Abs value of the min and max offsets that won't violate the assumption that
  // messages can always be placed in sequence order (in global time).
  std::pair<int64_t, int64_t> CalculateOffsetLimits() const;

 private:
  using Timestamp = uint64_t;
  using LogId = uint32_t;
  using TransactionId =
      uint32_t;  // LoggedIceCandidatePairEvent::transaction_id
  struct EventSequence final {
    EventSequence();
    ~EventSequence();
    LogId initiating_log;
    std::vector<absl::optional<Timestamp>> timestamps;
  };
  using EventSequences = std::unordered_map<TransactionId, EventSequence>;

  void AddEvents(const std::vector<LoggedIceCandidatePairEvent>& log,
                 LogId log_id);
  void CalculateOffsets();
  absl::optional<int64_t> CalculateOffset(
      const std::vector<absl::optional<Timestamp>>& sequence);

  EventSequences event_sequences_;
  std::vector<int64_t> offsets_;
  int64_t full_sequence_count_ = 0;

  // TODO(zstein): Interval tree would be more efficient here.
  std::vector<std::pair<int64_t, int64_t>> offset_limits_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_CLOCK_OFFSET_CALCULATOR_H_
