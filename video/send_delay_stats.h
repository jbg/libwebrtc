/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_SEND_DELAY_STATS_H_
#define VIDEO_SEND_DELAY_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <set>

#include "api/units/timestamp.h"
#include "call/video_send_stream.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "video/stats_counter.h"

namespace webrtc {

// Used to collect delay stats for video streams. The class gets callbacks
// from more than one threads and internally uses a mutex for data access
// synchronization.
// TODO(bugs.webrtc.org/11993): OnSendPacket and OnSentPacket will eventually
// be called consistently on the same thread. Once we're there, we should be
// able to avoid locking (at least for the fast path).
class SendDelayStats : public SendPacketObserver {
 public:
  explicit SendDelayStats(Clock* clock);
  ~SendDelayStats() override;

  // Adds the configured ssrcs for the rtp streams.
  // Stats will be calculated for these streams.
  void AddSsrcs(const VideoSendStream::Config& config,
                VideoEncoderConfig::ContentType content_type);

  // Called when a packet is sent (leaving socket).
  bool OnSentPacket(int packet_id, Timestamp time);

  //
  TimeDelta AverageSendDelayForTesting(uint32_t ssrc) const;
  TimeDelta AverageMaxDelayForTesting(uint32_t ssrc) const;

 protected:
  // From SendPacketObserver.
  // Called when a packet is sent to the transport.
  void OnSendPacket(uint16_t packet_id,
                    Timestamp capture_time,
                    uint32_t ssrc) override;

 private:
  // Map holding sent packets (mapped by sequence number).
  struct SequenceNumberOlderThan {
    bool operator()(uint16_t seq1, uint16_t seq2) const {
      return IsNewerSequenceNumber(seq2, seq1);
    }
  };

  // capture -> transport delay over SendSideDelayCounter::kWindow
  class SendSideDelayCounter {
   public:
    static constexpr TimeDelta kWindow = TimeDelta::Seconds(1);

    SendSideDelayCounter() = default;
    SendSideDelayCounter(const SendSideDelayCounter&) = delete;
    SendSideDelayCounter& operator=(const SendSideDelayCounter&) = delete;

    void Add(Timestamp now, TimeDelta delay);

    size_t num_samples() const { return num_samples_; }
    TimeDelta AvgAvgDelay() const { return sum_avg_ / num_samples_; }
    TimeDelta AvgMaxDelay() const { return sum_max_ / num_samples_; }

   private:
    struct SendDelayEntry {
      Timestamp send_time;
      TimeDelta value;
    };

    void RemoveOld(Timestamp now);

    std::deque<SendDelayEntry> delays_;
    TimeDelta sum_delay_ = TimeDelta::Zero();
    // Pointer to the max value in `delays_`, or nullptr when delays_.empty()
    TimeDelta* max_delay_ = nullptr;

    // Average avg_delay/max_delay over full duration.
    size_t num_samples_ = 0;
    TimeDelta sum_avg_ = TimeDelta::Zero();
    TimeDelta sum_max_ = TimeDelta::Zero();
  };
  struct SendDelayCounters {
    SendDelayCounters(bool is_screencast, Clock* clock)
        : is_screencast(is_screencast), send_delay(clock, nullptr, false) {}

    const bool is_screencast;

    // transport -> socket delay, reported as "SendDelayInMs"
    AvgCounter send_delay;

    // capture time -> transport delay, reported as "SendSideDelayInMs" and
    // "SendSideDelayMaxInMs"
    SendSideDelayCounter send_side_delay;
  };

  struct Packet {
    AvgCounter* send_delay_counter;
    Timestamp capture_time;
    Timestamp send_time;
  };

  typedef std::map<uint16_t, Packet, SequenceNumberOlderThan> PacketMap;

  void UpdateHistograms();
  void RemoveOld(Timestamp now, PacketMap* packets)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Clock* const clock_;
  Mutex mutex_;

  PacketMap packets_ RTC_GUARDED_BY(mutex_);
  size_t num_old_packets_ RTC_GUARDED_BY(mutex_);
  size_t num_skipped_packets_ RTC_GUARDED_BY(mutex_);

  // Mapped by SSRC.
  std::map<uint32_t, SendDelayCounters> send_delay_counters_
      RTC_GUARDED_BY(mutex_);
};

}  // namespace webrtc
#endif  // VIDEO_SEND_DELAY_STATS_H_
