/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/send_delay_stats.h"

#include <utility>

#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
// Packet with a larger delay are removed and excluded from the delay stats.
// Set to larger than max histogram delay which is 10 seconds.
constexpr TimeDelta kMaxSentPacketDelay = TimeDelta::Seconds(11);
constexpr size_t kMaxPacketMapSize = 2000;

// Limit for the maximum number of streams to calculate stats for.
constexpr size_t kMaxSsrcMapSize = 50;
constexpr int kMinRequiredPeriodicSamples = 5;
}  // namespace

SendDelayStats::SendDelayStats(Clock* clock)
    : clock_(clock), num_old_packets_(0), num_skipped_packets_(0) {}

SendDelayStats::~SendDelayStats() {
  if (num_old_packets_ > 0 || num_skipped_packets_ > 0) {
    RTC_LOG(LS_WARNING) << "Delay stats: number of old packets "
                        << num_old_packets_ << ", skipped packets "
                        << num_skipped_packets_ << ". Number of streams "
                        << send_delay_counters_.size();
  }
  UpdateHistograms();
}

void SendDelayStats::UpdateHistograms() {
  MutexLock lock(&mutex_);
  for (auto& [unused_ssrc, counters] : send_delay_counters_) {
    AggregatedStats stats = counters.send_delay.GetStats();
    if (stats.num_samples >= kMinRequiredPeriodicSamples) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.SendDelayInMs", stats.average);
      RTC_LOG(LS_INFO) << "WebRTC.Video.SendDelayInMs, " << stats.ToString();
    }

    if (counters.send_side_delay.num_samples() >= 200) {
      if (counters.is_screencast) {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.Screenshare.SendSideDelayInMs",
                                   counters.send_side_delay.AvgAvgDelay().ms());
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.Screenshare.SendSideDelayMaxInMs",
            counters.send_side_delay.AvgMaxDelay().ms());
      } else {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.SendSideDelayInMs",
                                   counters.send_side_delay.AvgAvgDelay().ms());
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.SendSideDelayMaxInMs",
                                   counters.send_side_delay.AvgMaxDelay().ms());
      }
    }
  }
}

void SendDelayStats::AddSsrcs(const VideoSendStream::Config& config,
                              VideoEncoderConfig::ContentType content_type) {
  MutexLock lock(&mutex_);
  if (send_delay_counters_.size() + config.rtp.ssrcs.size() > kMaxSsrcMapSize)
    return;

  bool is_screencast = content_type == VideoEncoderConfig::ContentType::kScreen;
  for (uint32_t ssrc : config.rtp.ssrcs) {
    send_delay_counters_.try_emplace(ssrc, is_screencast, clock_);
  }
}

void SendDelayStats::SendSideDelayCounter::Add(Timestamp now, TimeDelta delay) {
  // Replecating RtpSenderEgress::UpdateDelayStatistics with few improvemens
  RemoveOld(now);

  // Add new entry to the window.
  delays_.push_back({.send_time = now, .value = delay});
  sum_delay_ += delay;
  if (max_delay_ == nullptr || delay > *max_delay_) {
    max_delay_ = &delays_.back().value;
  }

  // Replecating SendStatisticsProxy::SendSideDelayUpdated.
  ++num_samples_;
  sum_avg_ += (sum_delay_ / delays_.size());
  sum_max_ += *max_delay_;
}

void SendDelayStats::SendSideDelayCounter::RemoveOld(Timestamp now) {
  Timestamp too_old = now - kWindow;
  while (!delays_.empty() && delays_.front().send_time < too_old) {
    sum_delay_ -= delays_.front().value;
    if (max_delay_ == &delays_.front().value) {
      max_delay_ = nullptr;
    }
    delays_.pop_front();
  }

  // Recompute max delay if previous max was pushed out of the window.
  if (max_delay_ == nullptr && !delays_.empty()) {
    max_delay_ = &delays_.front().value;
    for (SendDelayEntry& entry : delays_) {
      if (entry.value > *max_delay_) {
        max_delay_ = &entry.value;
      }
    }
  }
}

void SendDelayStats::OnSendPacket(uint16_t packet_id,
                                  Timestamp capture_time,
                                  uint32_t ssrc) {
  // Packet sent to transport.
  MutexLock lock(&mutex_);
  auto it = send_delay_counters_.find(ssrc);
  if (it == send_delay_counters_.end())
    return;

  Timestamp now = clock_->CurrentTime();
  RemoveOld(now, &packets_);

  // Replicating RtpSenderEgress::UpdateDelayStatistics: delay is accounted even
  // if packet is later dropped.
  it->second.send_side_delay.Add(now, capture_time - now);

  if (packets_.size() > kMaxPacketMapSize) {
    ++num_skipped_packets_;
    return;
  }

  packets_.emplace(packet_id,
                   Packet{.send_delay_counter = &it->second.send_delay,
                          .capture_time = capture_time,
                          .send_time = now});
}

bool SendDelayStats::OnSentPacket(int packet_id, Timestamp time) {
  // Packet leaving socket.
  if (packet_id == -1)
    return false;

  MutexLock lock(&mutex_);
  auto it = packets_.find(packet_id);
  if (it == packets_.end())
    return false;

  // Elapsed time from send (to transport) -> sent (leaving socket).
  TimeDelta diff = time - it->second.send_time;
  it->second.send_delay_counter->Add(diff.ms());
  packets_.erase(it);
  return true;
}

void SendDelayStats::RemoveOld(Timestamp now, PacketMap* packets) {
  while (!packets->empty()) {
    auto it = packets->begin();
    if (now - it->second.capture_time < kMaxSentPacketDelay)
      break;

    packets->erase(it);
    ++num_old_packets_;
  }
}

}  // namespace webrtc
