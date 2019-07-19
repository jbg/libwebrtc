/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const int64_t PacedSender::kMaxQueueLengthMs = 2000;
const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(Clock* clock,
                         PacketRouter* packet_router,
                         RtcEventLog* event_log,
                         const WebRtcKeyValueConfig* field_trials)
    : PacedSenderBase(clock, packet_router, event_log, field_trials),
      packet_router_(packet_router) {}

PacedSender::~PacedSender() = default;

void PacedSender::CreateProbeCluster(int bitrate_bps, int cluster_id) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::CreateProbeCluster(bitrate_bps, cluster_id);
}

void PacedSender::Pause() {
  {
    rtc::CritScope cs(&critsect_);
    PacedSenderBase::Pause();
  }

  rtc::CritScope cs(&process_thread_lock_);
  // Tell the process thread to call our TimeUntilNextProcess() method to get
  // a new (longer) estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::Resume() {
  {
    rtc::CritScope cs(&critsect_);
    PacedSenderBase::Resume();
  }

  rtc::CritScope cs(&process_thread_lock_);
  // Tell the process thread to call our TimeUntilNextProcess() method to
  // refresh the estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::SetCongestionWindow(int64_t congestion_window_bytes) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::SetCongestionWindow(congestion_window_bytes);
}

void PacedSender::UpdateOutstandingData(int64_t outstanding_bytes) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::UpdateOutstandingData(outstanding_bytes);
}

void PacedSender::SetProbingEnabled(bool enabled) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::SetProbingEnabled(enabled);
}

void PacedSender::SetPacingRates(uint32_t pacing_rate_bps,
                                 uint32_t padding_rate_bps) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::SetPacingRates(pacing_rate_bps, padding_rate_bps);
}

void PacedSender::InsertPacket(RtpPacketSender::Priority priority,
                               uint32_t ssrc,
                               uint16_t sequence_number,
                               int64_t capture_time_ms,
                               size_t bytes,
                               bool retransmission) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::InsertPacket(priority, ssrc, sequence_number,
                                capture_time_ms, bytes, retransmission);
}

void PacedSender::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::EnqueuePacket(std::move(packet));
}

void PacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::SetAccountForAudioPackets(account_for_audio);
}

int64_t PacedSender::ExpectedQueueTimeMs() const {
  rtc::CritScope cs(&critsect_);
  return PacedSenderBase::ExpectedQueueTimeMs();
}

size_t PacedSender::QueueSizePackets() const {
  rtc::CritScope cs(&critsect_);
  return PacedSenderBase::QueueSizePackets();
}

int64_t PacedSender::QueueSizeBytes() const {
  rtc::CritScope cs(&critsect_);
  return PacedSenderBase::QueueSizeBytes();
}

int64_t PacedSender::FirstSentPacketTimeMs() const {
  rtc::CritScope cs(&critsect_);
  return PacedSenderBase::FirstSentPacketTimeMs();
}

int64_t PacedSender::QueueInMs() const {
  rtc::CritScope cs(&critsect_);
  return PacedSenderBase::QueueInMs();
}

int64_t PacedSender::TimeUntilNextProcess() {
  rtc::CritScope cs(&critsect_);
  return PacedSenderBase::TimeUntilNextProcess();
}

size_t PacedSender::TimeToSendPadding(size_t bytes,
                                      const PacedPacketInfo& pacing_info) {
  critsect_.Leave();
  size_t bytes_sent = packet_router_->TimeToSendPadding(bytes, pacing_info);
  critsect_.Enter();
  return bytes_sent;
}

std::vector<std::unique_ptr<RtpPacketToSend>> PacedSender::GeneratePadding(
    size_t bytes) {
  critsect_.Leave();
  std::vector<std::unique_ptr<RtpPacketToSend>> padding =
      packet_router_->GeneratePadding(bytes);
  critsect_.Enter();
  return padding;
}

void PacedSender::SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                                const PacedPacketInfo& cluster_info) {
  critsect_.Leave();
  packet_router_->SendPacket(std::move(packet), cluster_info);
  critsect_.Enter();
}

RtpPacketSendResult PacedSender::TimeToSendPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    int64_t capture_timestamp,
    bool retransmission,
    const PacedPacketInfo& packet_info) {
  critsect_.Leave();
  RtpPacketSendResult result = packet_router_->TimeToSendPacket(
      ssrc, sequence_number, capture_timestamp, retransmission, packet_info);
  critsect_.Enter();
  return result;
}

void PacedSender::Process() {
  rtc::CritScope cs(&critsect_);
  ProcessPackets();
}

void PacedSender::ProcessThreadAttached(ProcessThread* process_thread) {
  RTC_LOG(LS_INFO) << "ProcessThreadAttached 0x" << process_thread;
  rtc::CritScope cs(&process_thread_lock_);
  process_thread_ = process_thread;
}

void PacedSender::SetQueueTimeLimit(int limit_ms) {
  rtc::CritScope cs(&critsect_);
  PacedSenderBase::SetQueueTimeLimit(limit_ms);
}

}  // namespace webrtc
