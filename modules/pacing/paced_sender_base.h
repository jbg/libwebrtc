/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACED_SENDER_BASE_H_
#define MODULES_PACING_PACED_SENDER_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/function_view.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "modules/include/module.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/pacing/packet_router.h"
#include "modules/pacing/round_robin_packet_queue.h"
#include "modules/rtp_rtcp/include/rtp_packet_pacer.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class Clock;
class RtcEventLog;

class PacedSenderBase : public RtpPacketPacer {
 public:
  static constexpr int64_t kNoCongestionWindow = -1;

  // Expected max pacer delay in ms. If ExpectedQueueTimeMs() is higher than
  // this value, the packet producers should wait (eg drop frames rather than
  // encoding them). Bitrate sent may temporarily exceed target set by
  // UpdateBitrate() so that this limit will be upheld.
  static const int64_t kMaxQueueLengthMs;
  // Pacing-rate relative to our target send rate.
  // Multiplicative factor that is applied to the target bitrate to calculate
  // the number of bytes that can be transmitted per interval.
  // Increasing this factor will result in lower delays in cases of bitrate
  // overshoots from the encoder.
  static const float kDefaultPaceMultiplier;

  PacedSenderBase(Clock* clock,
                  PacketRouter* packet_router,
                  RtcEventLog* event_log,
                  const WebRtcKeyValueConfig* field_trials = nullptr);

  ~PacedSenderBase() override;

  virtual void CreateProbeCluster(int bitrate_bps, int cluster_id);

  // Temporarily pause all sending.
  virtual void Pause();

  // Resume sending packets.
  virtual void Resume();

  virtual void SetCongestionWindow(int64_t congestion_window_bytes);
  virtual void UpdateOutstandingData(int64_t outstanding_bytes);

  // Enable bitrate probing. Enabled by default, mostly here to simplify
  // testing. Must be called before any packets are being sent to have an
  // effect.
  virtual void SetProbingEnabled(bool enabled);

  // Sets the pacing rates. Must be called once before packets can be sent.
  virtual void SetPacingRates(uint32_t pacing_rate_bps,
                              uint32_t padding_rate_bps);

  // Adds the packet information to the queue and calls TimeToSendPacket
  // when it's time to send.
  void InsertPacket(RtpPacketSender::Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override;

  // Adds the packet to the queue and calls PacketRouter::SendPacket() when
  // it's time to send.
  void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) override;

  // Currently audio traffic is not accounted by pacer and passed through.
  // With the introduction of audio BWE audio traffic will be accounted for
  // the pacer budget calculation. The audio traffic still will be injected
  // at high priority.
  void SetAccountForAudioPackets(bool account_for_audio) override;

  // Returns the time since the oldest queued packet was enqueued.
  virtual int64_t QueueInMs() const;

  virtual size_t QueueSizePackets() const;
  virtual int64_t QueueSizeBytes() const;

  // Returns the time when the first packet was sent, or -1 if no packet is
  // sent.
  virtual int64_t FirstSentPacketTimeMs() const;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  virtual int64_t ExpectedQueueTimeMs() const;

  virtual void SetQueueTimeLimit(int limit_ms);

 protected:
  // Process packets and return the time needed until the next packet can be
  // sent, if at all. Can be called early without problems.
  void ProcessPackets();
  absl::optional<int64_t> TimeUntilNextProbe();
  int64_t TimeUntilAvailableBudget();

  // TODO(sprang): Legacy, fix this.
  int64_t TimeUntilNextProcess();

  // TODO(sprang): Remove TimeToSendPadding.
  virtual size_t TimeToSendPadding(size_t bytes,
                                   const PacedPacketInfo& pacing_info) = 0;

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t bytes) = 0;

  virtual void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                             const PacedPacketInfo& cluster_info) = 0;

  virtual RtpPacketSendResult TimeToSendPacket(
      uint32_t ssrc,
      uint16_t sequence_number,
      int64_t capture_timestamp,
      bool retransmission,
      const PacedPacketInfo& packet_info) = 0;

 private:
  bool ShouldSendPacket(const RoundRobinPacketQueue::QueuedPacket& packet);

  int64_t UpdateTimeAndGetElapsedMs(int64_t now_us);
  bool ShouldSendKeepalive(int64_t at_time_us) const;

  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBudgetWithElapsedTime(int64_t delta_time_in_ms);
  void UpdateBudgetWithBytesSent(size_t bytes);

  size_t PaddingBytesToAdd(absl::optional<size_t> recommended_probe_size,
                           size_t bytes_sent);

  RoundRobinPacketQueue::QueuedPacket* GetPendingPacket(
      const PacedPacketInfo& pacing_info);
  void OnPacketSent(RoundRobinPacketQueue::QueuedPacket* packet);
  void OnPaddingSent(size_t padding_sent);

  bool Congested() const;
  int64_t TimeMilliseconds() const;

  Clock* const clock_;
  PacketRouter* const packet_router_;
  const std::unique_ptr<FieldTrialBasedConfig> fallback_field_trials_;
  const WebRtcKeyValueConfig* field_trials_;

  const bool drain_large_queues_;
  const bool send_padding_if_silent_;
  const bool pace_audio_;
  FieldTrialParameter<int> min_packet_limit_ms_;

  // TODO(webrtc:9716): Remove this when we are certain clocks are monotonic.
  // The last millisecond timestamp returned by |clock_|.
  mutable int64_t last_timestamp_ms_;
  bool paused_;
  // This is the media budget, keeping track of how many bits of media
  // we can pace out during the current interval.
  IntervalBudget media_budget_;
  // This is the padding budget, keeping track of how many bits of padding we're
  // allowed to send out during the current interval. This budget will be
  // utilized when there's no media to send.
  IntervalBudget padding_budget_;

  BitrateProber prober_;
  bool probing_send_failure_;

  uint32_t pacing_bitrate_kbps_;

  int64_t time_last_process_us_;
  int64_t last_send_time_us_;
  int64_t first_sent_packet_ms_;

  RoundRobinPacketQueue packets_;
  uint64_t packet_counter_;

  int64_t congestion_window_bytes_ = kNoCongestionWindow;
  int64_t outstanding_bytes_ = 0;

  int64_t queue_time_limit;
  bool account_for_audio_;

  // If true, PacedSender should only reference packets as in legacy mode.
  // If false, PacedSender may have direct ownership of RtpPacketToSend objects.
  // Defaults to true, will be changed to default false soon.
  const bool legacy_packet_referencing_;
};
}  // namespace webrtc
#endif  // MODULES_PACING_PACED_SENDER_BASE_H_
