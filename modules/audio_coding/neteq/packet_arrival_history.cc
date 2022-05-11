/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/packet_arrival_history.h"

#include <algorithm>

#include "api/neteq/tick_timer.h"
#include "modules/include/module_common_types_public.h"

namespace webrtc {

PacketArrivalHistory::PacketArrivalHistory(int window_size_ms)
    : window_size_ms_(window_size_ms) {}

void PacketArrivalHistory::Insert(uint32_t rtp_timestamp,
                                  int64_t arrival_time_ms) {
  RTC_DCHECK(sample_rate_khz_ > 0);
  int64_t unwrapped_rtp_timestamp_ms =
      timestamp_unwrapper_.Unwrap(rtp_timestamp) / sample_rate_khz_;
  history_.emplace_back(unwrapped_rtp_timestamp_ms, arrival_time_ms);
  MaybeUpdateCachedArrivals(history_.back());
  while (history_.front().rtp_timestamp_ms + window_size_ms_ <
         unwrapped_rtp_timestamp_ms) {
    if (&history_.front() == min_packet_arrival_) {
      min_packet_arrival_ = nullptr;
    }
    if (&history_.front() == max_packet_arrival_) {
      max_packet_arrival_ = nullptr;
    }
    history_.pop_front();
  }
  if (!min_packet_arrival_ || !max_packet_arrival_) {
    for (const PacketArrival& packet : history_) {
      MaybeUpdateCachedArrivals(packet);
    }
  }
}

void PacketArrivalHistory::MaybeUpdateCachedArrivals(
    const PacketArrival& packet_arrival) {
  if (!min_packet_arrival_ || packet_arrival <= *min_packet_arrival_) {
    min_packet_arrival_ = &packet_arrival;
  }
  if (!max_packet_arrival_ || packet_arrival >= *max_packet_arrival_) {
    max_packet_arrival_ = &packet_arrival;
  }
}

void PacketArrivalHistory::Reset() {
  history_.clear();
  min_packet_arrival_ = nullptr;
  max_packet_arrival_ = nullptr;
  timestamp_unwrapper_ = TimestampUnwrapper();
}

int PacketArrivalHistory::GetDelayMs(uint32_t rtp_timestamp,
                                     int64_t time_ms) const {
  RTC_DCHECK(sample_rate_khz_ > 0);
  if (history_.empty()) {
    return 0;
  }
  int64_t unwrapped_rtp_timestamp_ms =
      timestamp_unwrapper_.UnwrapWithoutUpdate(rtp_timestamp) /
      sample_rate_khz_;
  PacketArrival packet(unwrapped_rtp_timestamp_ms, time_ms);
  return std::max(static_cast<int>(packet - *min_packet_arrival_), 0);
}

int PacketArrivalHistory::GetMaxDelayMs() const {
  if (history_.empty()) {
    return 0;
  }
  return *max_packet_arrival_ - *min_packet_arrival_;
}

}  // namespace webrtc
