/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/source_tracker.h"

#include <algorithm>
#include <utility>

#include "rtc_base/trace_event.h"

namespace webrtc {

SourceTracker::SourceTracker(Clock* clock)
    : worker_thread_(TaskQueueBase::Current()), clock_(clock) {
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(clock_);
}

void SourceTracker::OnFrameDelivered(RtpPacketInfos packet_infos) {
  if (packet_infos.empty()) {
    return;
  }

  Timestamp now = clock_->CurrentTime();
  if (worker_thread_->IsCurrent()) {
    RTC_DCHECK_RUN_ON(worker_thread_);
    OnFrameDeliveredInternal(now, packet_infos);
  } else {
    worker_thread_->PostTask(
        SafeTask(worker_safety_.flag(),
                 [this, packet_infos = std::move(packet_infos), now]() {
                   RTC_DCHECK_RUN_ON(worker_thread_);
                   OnFrameDeliveredInternal(now, packet_infos);
                 }));
  }
}

void SourceTracker::OnFrameDeliveredInternal(
    Timestamp now,
    const RtpPacketInfos& packet_infos) {
  TRACE_EVENT0("webrtc", "SourceTracker::OnFrameDelivered");

  for (const RtpPacketInfo& packet_info : packet_infos) {
    for (uint32_t csrc : packet_info.csrcs()) {
      SourceKey key(RtpSourceType::CSRC, csrc);
      SourceEntry& entry = UpdateEntry(key);

      entry.timestamp = now;
      entry.audio_level = packet_info.audio_level();
      entry.absolute_capture_time = packet_info.absolute_capture_time();
      entry.local_capture_clock_offset =
          packet_info.local_capture_clock_offset();
      entry.rtp_timestamp = packet_info.rtp_timestamp();
    }

    SourceKey key(RtpSourceType::SSRC, packet_info.ssrc());
    SourceEntry& entry = UpdateEntry(key);

    entry.timestamp = now;
    entry.audio_level = packet_info.audio_level();
    entry.absolute_capture_time = packet_info.absolute_capture_time();
    entry.local_capture_clock_offset = packet_info.local_capture_clock_offset();
    entry.rtp_timestamp = packet_info.rtp_timestamp();

    FireAudioLevelCallback(key, entry);
  }

  PruneEntries(now);
}

std::vector<RtpSource> SourceTracker::GetSources() const {
  RTC_DCHECK_RUN_ON(worker_thread_);

  PruneEntries(clock_->CurrentTime());

  std::vector<RtpSource> sources;
  for (const auto& pair : list_) {
    const SourceKey& key = pair.first;
    const SourceEntry& entry = pair.second;

    sources.emplace_back(
        entry.timestamp, key.source, key.source_type, entry.rtp_timestamp,
        RtpSource::Extensions{
            .audio_level = entry.audio_level,
            .absolute_capture_time = entry.absolute_capture_time,
            .local_capture_clock_offset = entry.local_capture_clock_offset});
  }

  return sources;
}

void SourceTracker::SetAudioLevelCallback(
    uint32_t ssrc,
    absl::AnyInvocable<void(uint32_t, absl::optional<uint8_t>)>
        level_callback) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(level_callback);
  SourceKey key(RtpSourceType::SSRC, ssrc);
  RTC_DCHECK(level_callbacks_.find(key) == level_callbacks_.end());
  level_callbacks_.emplace(key, std::move(level_callback));
}

absl::AnyInvocable<void(uint32_t, absl::optional<uint8_t>)>
SourceTracker::RemoveAudioLevelCallback(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  SourceKey key(RtpSourceType::SSRC, ssrc);
  auto it = level_callbacks_.find(key);
  if (it == level_callbacks_.end())
    return nullptr;
  auto ret = std::move(it->second);
  level_callbacks_.erase(it);
  return ret;
}

SourceTracker::SourceEntry& SourceTracker::UpdateEntry(const SourceKey& key) {
  // We intentionally do |find() + emplace()|, instead of checking the return
  // value of `emplace()`, for performance reasons. It's much more likely for
  // the key to already exist than for it not to.
  auto map_it = map_.find(key);
  if (map_it == map_.end()) {
    // Insert a new entry at the front of the list.
    list_.emplace_front(key, SourceEntry());
    map_.emplace(key, list_.begin());
  } else if (map_it->second != list_.begin()) {
    // Move the old entry to the front of the list.
    list_.splice(list_.begin(), list_, map_it->second);
  }

  return list_.front().second;
}

void SourceTracker::PruneEntries(Timestamp now) const {
  Timestamp prune = now - kTimeout;
  while (!list_.empty() && list_.back().second.timestamp < prune) {
    const auto& key = list_.back().first;
    if (key.source_type == RtpSourceType::SSRC) {
      auto& entry = list_.back().second;
      entry.audio_level = absl::nullopt;  // Signal timeout.
      FireAudioLevelCallback(key, entry);
    }

    map_.erase(key);
    list_.pop_back();
  }
}

void SourceTracker::FireAudioLevelCallback(const SourceKey& key,
                                           const SourceEntry& entry) const {
  auto callback = level_callbacks_.find(key);
  if (callback != level_callbacks_.end())
    callback->second(entry.rtp_timestamp, entry.audio_level);
}

}  // namespace webrtc
