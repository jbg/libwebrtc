/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender_taskqueue.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {
class EnqueuePacketTask : public QueuedTask {
 public:
  EnqueuePacketTask(std::unique_ptr<RtpPacketToSend> packet,
                    PacedSenderTaskQueue* owner)
      : packet_(std::move(packet)), owner_(owner) {}

  bool Run() override {
    owner_->EnqueuePacket(std::move(packet_));
    return true;
  }

 private:
  std::unique_ptr<RtpPacketToSend> packet_;
  PacedSenderTaskQueue* const owner_;
};
}  // namespace

PacedSenderTaskQueue::PacedSenderTaskQueue(
    Clock* clock,
    PacketRouter* packet_router,
    RtcEventLog* event_log,
    const WebRtcKeyValueConfig* field_trials,
    TaskQueueFactory* task_queue_factory)
    : PacedSenderBase(clock, packet_router, event_log, field_trials),
      clock_(clock),
      packet_router_(packet_router),
      probe_started_(false),
      shutdown_(false),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "PacedSenderTaskQueue",
          TaskQueueFactory::Priority::HIGH)) {}

PacedSenderTaskQueue::~PacedSenderTaskQueue() {
  Shutdown();
}

void PacedSenderTaskQueue::CreateProbeCluster(int bitrate_bps, int cluster_id) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, bitrate_bps, cluster_id]() {
      CreateProbeCluster(bitrate_bps, cluster_id);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::CreateProbeCluster(bitrate_bps, cluster_id);
  MaybeProcessPackets(true);
}

void PacedSenderTaskQueue::Pause() {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this]() { Pause(); });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::Pause();
}

void PacedSenderTaskQueue::Resume() {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this]() { Resume(); });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::Resume();
  MaybeProcessPackets(false);
}

void PacedSenderTaskQueue::SetCongestionWindow(
    int64_t congestion_window_bytes) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, congestion_window_bytes]() {
      SetCongestionWindow(congestion_window_bytes);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  bool was_congested = Congested();
  PacedSenderBase::SetCongestionWindow(congestion_window_bytes);
  if (was_congested && !Congested()) {
    MaybeProcessPackets(false);
  }
}

void PacedSenderTaskQueue::UpdateOutstandingData(int64_t outstanding_bytes) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, outstanding_bytes]() {
      UpdateOutstandingData(outstanding_bytes);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  bool was_congested = Congested();
  PacedSenderBase::UpdateOutstandingData(outstanding_bytes);
  if (was_congested && !Congested()) {
    MaybeProcessPackets(false);
  }
}

void PacedSenderTaskQueue::SetProbingEnabled(bool enabled) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, enabled]() { SetProbingEnabled(enabled); });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::SetProbingEnabled(enabled);
}

void PacedSenderTaskQueue::SetPacingRates(uint32_t pacing_rate_bps,
                                          uint32_t padding_rate_bps) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, pacing_rate_bps, padding_rate_bps]() {
      SetPacingRates(pacing_rate_bps, padding_rate_bps);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::SetPacingRates(pacing_rate_bps, padding_rate_bps);
  MaybeProcessPackets(false);
}

void PacedSenderTaskQueue::InsertPacket(RtpPacketSender::Priority priority,
                                        uint32_t ssrc,
                                        uint16_t sequence_number,
                                        int64_t capture_time_ms,
                                        size_t bytes,
                                        bool retransmission) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, priority, ssrc, sequence_number,
                          capture_time_ms, bytes, retransmission]() {
      InsertPacket(priority, ssrc, sequence_number, capture_time_ms, bytes,
                   retransmission);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::InsertPacket(priority, ssrc, sequence_number,
                                capture_time_ms, bytes, retransmission);
  if (PacedSenderBase::QueueSizePackets() == 1) {
    MaybeProcessPackets(false);
  }
}

void PacedSenderTaskQueue::EnqueuePacket(
    std::unique_ptr<RtpPacketToSend> packet) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask(
        absl::make_unique<EnqueuePacketTask>(std::move(packet), this));
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::EnqueuePacket(std::move(packet));
  if (PacedSenderBase::QueueSizePackets() == 1) {
    MaybeProcessPackets(false);
  }
}

void PacedSenderTaskQueue::SetAccountForAudioPackets(bool account_for_audio) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, account_for_audio]() {
      SetAccountForAudioPackets(account_for_audio);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::SetAccountForAudioPackets(account_for_audio);
}

void PacedSenderTaskQueue::SetQueueTimeLimit(int limit_ms) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, limit_ms]() { SetQueueTimeLimit(limit_ms); });
    return;
  }
  RTC_DCHECK_RUN_ON(&task_queue_);
  PacedSenderBase::SetQueueTimeLimit(limit_ms);
}

int64_t PacedSenderTaskQueue::ExpectedQueueTimeMs() const {
  return GetStats().expected_queue_time_ms;
}

size_t PacedSenderTaskQueue::QueueSizePackets() const {
  return GetStats().queue_size_packets;
}

int64_t PacedSenderTaskQueue::QueueSizeBytes() const {
  return GetStats().queue_size_bytes;
}

int64_t PacedSenderTaskQueue::FirstSentPacketTimeMs() const {
  return GetStats().first_sent_packet_time_ms;
}

int64_t PacedSenderTaskQueue::QueueInMs() const {
  return GetStats().queue_in_ms;
}

void PacedSenderTaskQueue::MaybeProcessPackets(bool is_probe) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  if (IsShutdown()) {
    return;
  }

  // If we're probing, only process packets when probe timer tells us to.
  if (probe_started_ && !is_probe) {
    return;
  }

  ProcessPackets();

  auto time_until_probe = TimeUntilNextProbe();
  int64_t time_to_next_process =
      time_until_probe.value_or(TimeUntilAvailableBudget());
  probe_started_ = time_until_probe.has_value();
  time_to_next_process = std::max<int64_t>(0, time_to_next_process);

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (next_scheduled_process_.value_or(now_ms) <= now_ms) {
    next_scheduled_process_.reset();
  }

  int64_t next_process_ms = now_ms + time_to_next_process;
  if (probe_started_ || !next_scheduled_process_ ||
      *next_scheduled_process_ > next_process_ms) {
    next_scheduled_process_ = next_process_ms;
    task_queue_.PostDelayedTask(
        [this]() { MaybeProcessPackets(probe_started_); },
        time_to_next_process);

    rtc::CritScope cs(&crit_);
    current_stats_.expected_queue_time_ms =
        PacedSenderBase::ExpectedQueueTimeMs();
    current_stats_.first_sent_packet_time_ms =
        PacedSenderBase::FirstSentPacketTimeMs();
    current_stats_.queue_in_ms = PacedSenderBase::QueueInMs();
    current_stats_.queue_size_bytes = PacedSenderBase::QueueSizeBytes();
    current_stats_.queue_size_packets = PacedSenderBase::QueueSizePackets();
    current_stats_.first_sent_packet_time_ms =
        PacedSenderBase::FirstSentPacketTimeMs();
  }
}

size_t PacedSenderTaskQueue::TimeToSendPadding(
    size_t bytes,
    const PacedPacketInfo& pacing_info) {
  return packet_router_->TimeToSendPadding(bytes, pacing_info);
}

std::vector<std::unique_ptr<RtpPacketToSend>>
PacedSenderTaskQueue::GeneratePadding(size_t bytes) {
  return packet_router_->GeneratePadding(bytes);
}

void PacedSenderTaskQueue::SendRtpPacket(
    std::unique_ptr<RtpPacketToSend> packet,
    const PacedPacketInfo& cluster_info) {
  packet_router_->SendPacket(std::move(packet), cluster_info);
}

RtpPacketSendResult PacedSenderTaskQueue::TimeToSendPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    int64_t capture_timestamp,
    bool retransmission,
    const PacedPacketInfo& packet_info) {
  return packet_router_->TimeToSendPacket(
      ssrc, sequence_number, capture_timestamp, retransmission, packet_info);
}

PacedSenderTaskQueue::Stats PacedSenderTaskQueue::GetStats() const {
  rtc::CritScope cs(&crit_);
  return current_stats_;
}

void PacedSenderTaskQueue::Shutdown() {
  rtc::CritScope cs(&crit_);
  shutdown_ = true;
}

bool PacedSenderTaskQueue::IsShutdown() const {
  rtc::CritScope cs(&crit_);
  return shutdown_;
}

}  // namespace webrtc
