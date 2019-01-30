/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/clock_offset_calculator.h"

#include <algorithm>

#include "rtc_base/logging.h"

namespace webrtc {

ClockOffsetCalculator::ClockOffsetCalculator() = default;
ClockOffsetCalculator::~ClockOffsetCalculator() = default;

void ClockOffsetCalculator::ProcessLogs(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events) {
  AddEvents(log1_events, 1);
  AddEvents(log2_events, 2);
  CalculateOffsets();
}

int64_t ClockOffsetCalculator::CalculateMean() const {
  if (offsets_.size() == 0) {
    return 0;
  }

  int64_t offset_sum = 0;
  for (const auto& offset : offsets_) {
    offset_sum += offset;
  }
  return offset_sum / offsets_.size();
}

int64_t ClockOffsetCalculator::CalculateMedian() {
  if (offsets_.size() == 0) {
    return 0;
  }

  std::sort(offsets_.begin(), offsets_.end());

  auto index = offsets_.size() / 2;
  auto median = offsets_[index];
  if (offsets_.size() % 2 == 0) {
    median += offsets_[index - 1];
    median /= 2;
  }
  return median;
}

int64_t ClockOffsetCalculator::FullSequenceCount() const {
  return offsets_.size();
}

void ClockOffsetCalculator::AddEvents(
    const std::vector<LoggedIceCandidatePairEvent>& events,
    LogId log_id) {
  for (const auto& event : events) {
    EventSequence& es = event_sequences_[event.transaction_id];
    if (event.type == IceCandidatePairEventType::kCheckSent) {
      es.initiating_log = log_id;
    }
    absl::optional<Timestamp>& t =
        es.timestamps[static_cast<size_t>(event.type)];
    if (t) {
      RTC_LOG(LS_WARNING) << "Double set timestamp for transaction id "
                          << event.transaction_id;
    }
    t.emplace(event.timestamp_us);
  }
}

ClockOffsetCalculator::EventSequence::EventSequence()
    : initiating_log(0), timestamps(4) {}

ClockOffsetCalculator::EventSequence::~EventSequence() = default;

absl::optional<int64_t> ClockOffsetCalculator::CalculateOffset(
    const std::vector<absl::optional<Timestamp>>& timestamps) {
  if (!timestamps[0] || !timestamps[1] || !timestamps[2] || !timestamps[3]) {
    return absl::optional<int64_t>();
  }

  const int64_t total_time = static_cast<int64_t>(*timestamps[3]) -
                             static_cast<int64_t>(*timestamps[0]);
  const int64_t processing_time = static_cast<int64_t>(*timestamps[2]) -
                                  static_cast<int64_t>(*timestamps[1]);
  const int64_t expected_receive =
      static_cast<int64_t>(*timestamps[0]) + (total_time - processing_time) / 2;
  const int64_t offset =
      static_cast<int64_t>(*timestamps[1]) - expected_receive;
  return offset;
}

void ClockOffsetCalculator::CalculateOffsets() {
  for (const auto& p : event_sequences_) {
    const EventSequence& sequence = p.second;
    auto offset = CalculateOffset(sequence.timestamps);
    if (offset) {
      if (sequence.initiating_log == 2) {
        offset = -*offset;
      }
      offsets_.push_back(*offset);
    }
  }
}

}  // namespace webrtc
