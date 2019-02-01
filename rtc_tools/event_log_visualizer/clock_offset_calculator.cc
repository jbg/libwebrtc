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

void ClockOffsetCalculator::ProcessTransactions(
    const IceTransactions& transactions) {
  for (const auto& p : transactions.ice_transactions) {
    const IceTransaction& transaction = p.second;
    if (transaction.stage_reached() != 4) {
      continue;
    }

    const int64_t ping_sent_ms = transaction.ping_sent->log_time_ms;
    const int64_t ping_received_ms = transaction.ping_received->log_time_ms;
    const int64_t response_sent_ms = transaction.response_sent->log_time_ms;
    const int64_t response_received_ms =
        transaction.response_received->log_time_ms;

    const int64_t total_time_ms = response_received_ms - ping_sent_ms;
    const int64_t processing_time_ms = response_sent_ms - ping_received_ms;
    const int64_t expected_receive_ms =
        ping_sent_ms + (total_time_ms - processing_time_ms) / 2;
    int64_t offset_ms = ping_received_ms - expected_receive_ms;
    if (transaction.ping_sent->log_id == kLogId1) {
      offset_ms = -offset_ms;
    }
    offsets_ms_.push_back(offset_ms);
  }
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

}  // namespace webrtc
