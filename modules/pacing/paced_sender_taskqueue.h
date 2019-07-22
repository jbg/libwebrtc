/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACED_SENDER_TASKQUEUE_H_
#define MODULES_PACING_PACED_SENDER_TASKQUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "api/task_queue/task_queue_factory.h"
#include "modules/include/module.h"
#include "modules/pacing/paced_sender_base.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class Clock;
class RtcEventLog;

class PacedSenderTaskQueue : public PacedSenderBase {
 public:
  struct Stats {
    int64_t queue_in_ms;
    size_t queue_size_packets;
    int64_t queue_size_bytes;
    int64_t expected_queue_time_ms;
    int64_t first_sent_packet_time_ms;
  };

  PacedSenderTaskQueue(Clock* clock,
                       PacketRouter* packet_router,
                       RtcEventLog* event_log,
                       const WebRtcKeyValueConfig* field_trials,
                       TaskQueueFactory* task_queue_factory);

  ~PacedSenderTaskQueue() override;

  void CreateProbeCluster(int bitrate_bps, int cluster_id) override;

  // Temporarily pause all sending.
  void Pause() override;

  // Resume sending packets.
  void Resume() override;

  void SetCongestionWindow(int64_t congestion_window_bytes) override;
  void UpdateOutstandingData(int64_t outstanding_bytes) override;

  // Enable bitrate probing. Enabled by default, mostly here to simplify
  // testing. Must be called before any packets are being sent to have an
  // effect.
  void SetProbingEnabled(bool enabled) override;

  // Sets the pacing rates. Must be called once before packets can be sent.
  void SetPacingRates(uint32_t pacing_rate_bps,
                      uint32_t padding_rate_bps) override;

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

  void SetQueueTimeLimit(int limit_ms) override;

  // Returns the time since the oldest queued packet was enqueued.
  int64_t QueueInMs() const override;

  size_t QueueSizePackets() const override;
  int64_t QueueSizeBytes() const override;

  // Returns the time when the first packet was sent, or -1 if no packet is
  // sent.
  int64_t FirstSentPacketTimeMs() const override;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  int64_t ExpectedQueueTimeMs() const override;

  Stats GetStats() const;

 protected:
  void MaybeProcessPackets(bool is_probe);

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& pacing_info) override
      RTC_RUN_ON(task_queue_);

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t bytes) override RTC_RUN_ON(task_queue_);

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& cluster_info) override
      RTC_RUN_ON(task_queue_);

  RtpPacketSendResult TimeToSendPacket(
      uint32_t ssrc,
      uint16_t sequence_number,
      int64_t capture_timestamp,
      bool retransmission,
      const PacedPacketInfo& packet_info) override RTC_RUN_ON(task_queue_);

 private:
  void Shutdown();
  bool IsShutdown() const;

  Clock* const clock_;
  PacketRouter* const packet_router_;
  absl::optional<int64_t> next_scheduled_process_;
  bool probe_started_;
  rtc::CriticalSection crit_;
  bool shutdown_ RTC_GUARDED_BY(crit_);
  Stats current_stats_ RTC_GUARDED_BY(crit_);
  rtc::TaskQueue task_queue_;
};
}  // namespace webrtc
#endif  // MODULES_PACING_PACED_SENDER_TASKQUEUE_H_
