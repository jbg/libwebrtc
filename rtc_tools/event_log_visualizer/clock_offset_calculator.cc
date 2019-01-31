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

namespace {

const uint32_t kLogId1 = 1;
const uint32_t kLogId2 = 2;

}  // namespace

ClockOffsetCalculator::ClockOffsetCalculator() = default;
ClockOffsetCalculator::~ClockOffsetCalculator() = default;

void ClockOffsetCalculator::ProcessLogs(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events) {
  AddEvents(log1_events, kLogId1);
  AddEvents(log2_events, kLogId2);
  CalculateOffsets();
}

int64_t ClockOffsetCalculator::CalculateMeanOffsetMs() const {
  if (offsets_ms_.size() == 0) {
    return 0;
  }

  int64_t offset_sum_ms = 0;
  for (const auto& offset_ms : offsets_ms_) {
    offset_sum_ms += offset_ms;
  }
  return offset_sum_ms / offsets_ms_.size();
}

int64_t ClockOffsetCalculator::CalculateMedianOffsetMs() {
  if (offsets_ms_.size() == 0) {
    return 0;
  }

  std::sort(offsets_ms_.begin(), offsets_ms_.end());

  auto index = offsets_ms_.size() / 2;
  auto median_ms = offsets_ms_[index];
  if (offsets_ms_.size() % 2 == 0) {
    median_ms += offsets_ms_[index - 1];
    median_ms /= 2;
  }
  return median_ms;
}

int64_t ClockOffsetCalculator::FullSequenceCount() const {
  return offsets_ms_.size();
}

void ClockOffsetCalculator::AddEvents(
    const std::vector<LoggedIceCandidatePairEvent>& events,
    LogId log_id) {
  for (const auto& event : events) {
    EventSequence& es = event_sequences_[event.transaction_id];
    if (event.type == IceCandidatePairEventType::kCheckSent) {
      es.initiating_log = log_id;
    }
    absl::optional<uint64_t>& timestamp_ms =
        es.timestamps_ms[static_cast<size_t>(event.type)];
    if (timestamp_ms) {
      RTC_LOG(LS_WARNING) << "Double set timestamp for transaction id "
                          << event.transaction_id;
    }
    timestamp_ms.emplace(event.log_time_ms());
  }
}

ClockOffsetCalculator::EventSequence::EventSequence()
    : initiating_log(0), timestamps_ms(4) {}

ClockOffsetCalculator::EventSequence::~EventSequence() = default;

absl::optional<int64_t> ClockOffsetCalculator::CalculateOffsetMs(
    const std::vector<absl::optional<uint64_t>>& timestamps_ms) {
  if (!timestamps_ms[0] || !timestamps_ms[1] || !timestamps_ms[2] ||
      !timestamps_ms[3]) {
    return absl::optional<int64_t>();
  }

  const int64_t total_time_ms = static_cast<int64_t>(*timestamps_ms[3]) -
                                static_cast<int64_t>(*timestamps_ms[0]);
  const int64_t processing_time_ms = static_cast<int64_t>(*timestamps_ms[2]) -
                                     static_cast<int64_t>(*timestamps_ms[1]);
  const int64_t expected_receive_ms = static_cast<int64_t>(*timestamps_ms[0]) +
                                      (total_time_ms - processing_time_ms) / 2;
  const int64_t offset_ms =
      static_cast<int64_t>(*timestamps_ms[1]) - expected_receive_ms;
  return offset_ms;
}

void ClockOffsetCalculator::CalculateOffsets() {
  for (const auto& p : event_sequences_) {
    const EventSequence& sequence = p.second;
    auto offset_ms = CalculateOffsetMs(sequence.timestamps_ms);
    if (offset_ms) {
      if (sequence.initiating_log == 2) {
        offset_ms = -*offset_ms;
      }
      offsets_ms_.push_back(*offset_ms);
    }
  }
}

}  // namespace webrtc
