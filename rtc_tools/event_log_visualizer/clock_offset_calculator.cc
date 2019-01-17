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
  AddEvents(log1_events);
  AddEvents(log2_events);
  CalculateOffsets();
}

int64_t ClockOffsetCalculator::CalculateMean() const {
  if (full_sequence_count_ == 0) {
    return 0;
  }

  int64_t offset_sum = 0;
  for (const auto& offset : offsets_) {
    offset_sum += offset;
  }
  return offset_sum / full_sequence_count_;
}

int64_t ClockOffsetCalculator::CalculateMedian() {
  if (offsets_.size() == 0) {
    return 0;
  }

  std::sort(offsets_.begin(), offsets_.end());
  auto median = offsets_[offsets_.size() / 2];
  return median;
}

void ClockOffsetCalculator::AddEvents(
    const std::vector<LoggedIceCandidatePairEvent>& events) {
  for (const auto& event : events) {
    EventSequence& es = event_sequences_[event.transaction_id];
    absl::optional<Timestamp>& t =
        es.timestamps[static_cast<size_t>(event.type)];
    if (t) {
      // TODO(zstein): Warn double set.
      RTC_LOG(LS_WARNING) << "Double set timestamp for transaction id "
                          << event.transaction_id;
    }
    t.emplace(event.timestamp_us);
  }
}

ClockOffsetCalculator::EventSequence::EventSequence() : timestamps(4) {}

ClockOffsetCalculator::EventSequence::~EventSequence() = default;

absl::optional<int64_t> ClockOffsetCalculator::CalculateOffset(
    const std::vector<absl::optional<Timestamp>>& timestamps) {
  // TODO(zstein): Calculate valid offsets here.

  if (!timestamps[0] || !timestamps[1] || !timestamps[2] || !timestamps[3]) {
    return absl::optional<int64_t>();
  }
  const int64_t total_time = *timestamps[3] - *timestamps[0];
  const int64_t processing_time = *timestamps[2] - *timestamps[1];
  const int64_t expected_receive =
      *timestamps[0] + (total_time - processing_time) / 2;
  const int64_t offset = *timestamps[1] - expected_receive;
  return offset;
}

void ClockOffsetCalculator::CalculateOffsets() {
  for (const auto& p : event_sequences_) {
    const EventSequence& sequence = p.second;
    auto offset = CalculateOffset(sequence.timestamps);
    if (offset) {
      offsets_.push_back(*offset);
      full_sequence_count_ += 1;
    }
  }
}

}  // namespace webrtc
