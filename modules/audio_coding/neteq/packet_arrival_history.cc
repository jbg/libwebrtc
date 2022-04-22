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

PacketArrivalHistory::PacketArrivalHistory(int window_size_ms,
                                           TickTimer* tick_timer)
    : window_size_ms_(window_size_ms), timer_(tick_timer->GetNewStopwatch()) {}

void PacketArrivalHistory::Insert(uint32_t rtp_timestamp) {
  RTC_DCHECK(sample_rate_khz_ > 0);
  int64_t unwrapped_rtp_timestamp_ms =
      timestamp_unwrapper_.Unwrap(rtp_timestamp) / sample_rate_khz_;
  history_.emplace_back(unwrapped_rtp_timestamp_ms, timer_->ElapsedMs());
  MaybeUpdateCachedArrivals(history_.back());
  while (history_.front().rtp_timestamp_ms + window_size_ms_ <
         unwrapped_rtp_timestamp_ms) {
    if (history_.front() == earliest_arrival_) {
      earliest_arrival_.reset();
    }
    if (history_.front() == latest_arrival_) {
      latest_arrival_.reset();
    }
    history_.pop_front();
  }
  if (!earliest_arrival_ || !latest_arrival_) {
    for (const PacketArrival& packet : history_) {
      MaybeUpdateCachedArrivals(packet);
    }
  }
}

void PacketArrivalHistory::MaybeUpdateCachedArrivals(
    const PacketArrival& packet) {
  if (!earliest_arrival_ || packet - *earliest_arrival_ <= 0) {
    earliest_arrival_ = packet;
  }
  if (!latest_arrival_ || packet - *latest_arrival_ >= 0) {
    latest_arrival_ = packet;
  }
}

void PacketArrivalHistory::Reset() {
  history_.clear();
  earliest_arrival_.reset();
  latest_arrival_.reset();
  timestamp_unwrapper_ = TimestampUnwrapper();
}

int PacketArrivalHistory::GetDelayMs(uint32_t rtp_timestamp) const {
  RTC_DCHECK(sample_rate_khz_ > 0);
  if (history_.empty()) {
    return 0;
  }
  int64_t unwrapped_rtp_timestamp_ms =
      timestamp_unwrapper_.UnwrapWithoutUpdate(rtp_timestamp) /
      sample_rate_khz_;
  PacketArrival packet(unwrapped_rtp_timestamp_ms, timer_->ElapsedMs());
  return std::max(static_cast<int>(packet - *earliest_arrival_), 0);
}

int PacketArrivalHistory::GetMaxDelayMs() const {
  RTC_DCHECK(sample_rate_khz_ > 0);
  if (history_.empty()) {
    return 0;
  }
  return *latest_arrival_ - *earliest_arrival_;
}

}  // namespace webrtc
