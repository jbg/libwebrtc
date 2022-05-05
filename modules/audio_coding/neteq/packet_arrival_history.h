/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_PACKET_ARRIVAL_HISTORY_H_
#define MODULES_AUDIO_CODING_NETEQ_PACKET_ARRIVAL_HISTORY_H_

#include <cstdint>
#include <deque>
#include <memory>

#include "api/neteq/tick_timer.h"
#include "modules/include/module_common_types_public.h"

namespace webrtc {

// Stores timing information about previously received packets.
// The history has a fixed size window where old data is automatically pruned.
// Packet delay is caluclated based on a reference packet `a` which has arrived
// with the lowest delay. The delay for packet `b` is calculated as
// `(b.arrival_time - a.arrival_time) - (b.rtp_timestamp - a.rtp_timestamp)`.
class PacketArrivalHistory {
 public:
  PacketArrivalHistory(int window_size_ms, TickTimer* timer);

  // Insert packet with rtp_timestamp into the history. The arrival time is
  // recorded as the current time.
  void Insert(uint32_t rtp_timestamp);

  // Get delay for `rtp_timestamp` relative to the earliest packet arrival in
  // the history.
  int GetDelayMs(uint32_t rtp_timestamp) const;

  // Get the maximum packet delay observed in the history.
  int GetMaxDelayMs() const;

  void Reset();

  void set_sample_rate(int sample_rate) {
    sample_rate_khz_ = sample_rate / 1000;
  }

 private:
  struct PacketArrival {
    PacketArrival(uint32_t rtp_timestamp_ms, int arrival_time_ms)
        : rtp_timestamp_ms(rtp_timestamp_ms),
          arrival_time_ms(arrival_time_ms) {}
    // Unwrapped rtp timestamp scaled to milliseconds.
    int64_t rtp_timestamp_ms;
    int64_t arrival_time_ms;
    bool operator==(const PacketArrival& other) const {
      return rtp_timestamp_ms == other.rtp_timestamp_ms &&
             arrival_time_ms == other.arrival_time_ms;
    }
    int64_t operator-(const PacketArrival& other) const {
      return arrival_time_ms - other.arrival_time_ms -
             (rtp_timestamp_ms - other.rtp_timestamp_ms);
    }
  };
  std::deque<PacketArrival> history_;
  // Updates `earliest_arrival_` and `latest_arrival_`.
  void MaybeUpdateCachedArrivals(const PacketArrival& packet);
  absl::optional<PacketArrival> earliest_arrival_;
  absl::optional<PacketArrival> latest_arrival_;
  const int window_size_ms_;
  const std::unique_ptr<TickTimer::Stopwatch> timer_;
  TimestampUnwrapper timestamp_unwrapper_;
  int sample_rate_khz_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_PACKET_ARRIVAL_HISTORY_H_
