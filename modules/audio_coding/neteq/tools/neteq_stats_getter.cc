/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_stats_getter.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace test {

std::string NetEqStatsGetter::ConcealmentEvent::ToString() const {
  char ss_buf[256];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss << "ConcealmentEvent duration_ms:" << duration_ms
     << " event_number:" << concealment_event_number
     << " time_from_previous_event_end_ms:" << time_from_previous_event_end_ms;
  return ss.str();
}

NetEqStatsGetter::NetEqStatsGetter(
    std::unique_ptr<NetEqDelayAnalyzer> delay_analyzer)
    : delay_analyzer_(std::move(delay_analyzer)) {}

void NetEqStatsGetter::BeforeGetAudio(NetEq* neteq) {
  if (delay_analyzer_) {
    delay_analyzer_->BeforeGetAudio(neteq);
  }
}

void NetEqStatsGetter::AfterGetAudio(int64_t time_now_ms,
                                     const AudioFrame& audio_frame,
                                     bool muted,
                                     NetEq* neteq) {
  if (!neteq->LastDecodedTimestamps().empty()) {
    last_decoded_packet_time_ms_ = time_now_ms;
  }

  // TODO(minyue): Get stats should better not be called as a call back after
  // get audio. It is called independently from get audio in practice.
  const auto lifetime_stat = neteq->GetLifetimeStatistics();
  if (last_stats_query_time_ms_ == 0 ||
      rtc::TimeDiff(time_now_ms, last_stats_query_time_ms_) >=
          stats_query_interval_ms_) {
    NetEqNetworkStatistics stats;
    RTC_CHECK_EQ(neteq->NetworkStatistics(&stats), 0);
    if (rtc::TimeDiff(time_now_ms, last_decoded_packet_time_ms_) < 10000) {
      // Only record stats if we have decoded packets in the last 10 seconds.
      stats_.push_back(std::make_pair(time_now_ms, stats));
      RecordLifetimeStats(time_now_ms, lifetime_stat);
    }
    last_stats_query_time_ms_ = time_now_ms;
    last_lifetime_stats_ = lifetime_stat;
  }

  const auto voice_concealed_samples =
      lifetime_stat.concealed_samples - lifetime_stat.silent_concealed_samples;
  if (current_concealment_event_ != lifetime_stat.concealment_events &&
      voice_concealed_samples_until_last_event_ < voice_concealed_samples) {
    if (last_event_end_time_ms_ > 0) {
      // Do not account for the first event to avoid start of the call
      // skewing.
      ConcealmentEvent concealment_event;
      uint64_t last_event_voice_concealed_samples =
          voice_concealed_samples - voice_concealed_samples_until_last_event_;
      RTC_CHECK_GT(last_event_voice_concealed_samples, 0);
      concealment_event.duration_ms = last_event_voice_concealed_samples /
                                      (audio_frame.sample_rate_hz_ / 1000);
      concealment_event.concealment_event_number = current_concealment_event_;
      concealment_event.time_from_previous_event_end_ms =
          time_now_ms - last_event_end_time_ms_;
      concealment_events_.emplace_back(concealment_event);
      voice_concealed_samples_until_last_event_ = voice_concealed_samples;
    }
    last_event_end_time_ms_ = time_now_ms;
    voice_concealed_samples_until_last_event_ = voice_concealed_samples;
    current_concealment_event_ = lifetime_stat.concealment_events;
  }

  if (delay_analyzer_) {
    delay_analyzer_->AfterGetAudio(time_now_ms, audio_frame, muted, neteq);
  }
}

double NetEqStatsGetter::AverageSpeechExpandRate() const {
  double sum_speech_expand = std::accumulate(
      stats_.begin(), stats_.end(), double{0.0},
      [](double a, std::pair<int64_t, NetEqNetworkStatistics> b) {
        return a + static_cast<double>(b.second.speech_expand_rate);
      });
  return sum_speech_expand / 16384.0 / stats_.size();
}

NetEqStatsGetter::Stats NetEqStatsGetter::AverageStats() const {
  Stats sum_stats = std::accumulate(
      stats_.begin(), stats_.end(), Stats(),
      [](Stats a, std::pair<int64_t, NetEqNetworkStatistics> bb) {
        const auto& b = bb.second;
        a.current_buffer_size_ms += b.current_buffer_size_ms;
        a.preferred_buffer_size_ms += b.preferred_buffer_size_ms;
        a.jitter_peaks_found += b.jitter_peaks_found;
        a.packet_loss_rate += b.packet_loss_rate / 16384.0;
        a.expand_rate += b.expand_rate / 16384.0;
        a.speech_expand_rate += b.speech_expand_rate / 16384.0;
        a.preemptive_rate += b.preemptive_rate / 16384.0;
        a.accelerate_rate += b.accelerate_rate / 16384.0;
        a.secondary_decoded_rate += b.secondary_decoded_rate / 16384.0;
        a.secondary_discarded_rate += b.secondary_discarded_rate / 16384.0;
        a.clockdrift_ppm += b.clockdrift_ppm;
        a.added_zero_samples += b.added_zero_samples;
        a.mean_waiting_time_ms += b.mean_waiting_time_ms;
        a.median_waiting_time_ms += b.median_waiting_time_ms;
        a.min_waiting_time_ms = std::min(
            a.min_waiting_time_ms, static_cast<double>(b.min_waiting_time_ms));
        a.max_waiting_time_ms = std::max(
            a.max_waiting_time_ms, static_cast<double>(b.max_waiting_time_ms));
        return a;
      });

  sum_stats.current_buffer_size_ms /= stats_.size();
  sum_stats.preferred_buffer_size_ms /= stats_.size();
  sum_stats.jitter_peaks_found /= stats_.size();
  sum_stats.packet_loss_rate /= stats_.size();
  sum_stats.expand_rate /= stats_.size();
  sum_stats.speech_expand_rate /= stats_.size();
  sum_stats.preemptive_rate /= stats_.size();
  sum_stats.accelerate_rate /= stats_.size();
  sum_stats.secondary_decoded_rate /= stats_.size();
  sum_stats.secondary_discarded_rate /= stats_.size();
  sum_stats.clockdrift_ppm /= stats_.size();
  sum_stats.added_zero_samples /= stats_.size();
  sum_stats.mean_waiting_time_ms /= stats_.size();
  sum_stats.median_waiting_time_ms /= stats_.size();

  return sum_stats;
}

// In order to be able to stop counting the stats during periods when no packets
// are received, we need to incrementally add the difference since the last time
// we sampled the stats.
void NetEqStatsGetter::RecordLifetimeStats(
    int64_t time_now_ms,
    const NetEqLifetimeStatistics& stats) {
  NetEqLifetimeStatistics lifetime_stats;
  if (!lifetime_stats_.empty()) {
    lifetime_stats = lifetime_stats_.back().second;
  }

  lifetime_stats.total_samples_received +=
      stats.total_samples_received -
      last_lifetime_stats_.total_samples_received;
  lifetime_stats.concealed_samples +=
      stats.concealed_samples - last_lifetime_stats_.concealed_samples;
  lifetime_stats.concealment_events +=
      stats.concealment_events - last_lifetime_stats_.concealment_events;
  lifetime_stats.jitter_buffer_delay_ms +=
      stats.jitter_buffer_delay_ms -
      last_lifetime_stats_.jitter_buffer_delay_ms;
  lifetime_stats.jitter_buffer_emitted_count +=
      stats.jitter_buffer_emitted_count -
      last_lifetime_stats_.jitter_buffer_emitted_count;
  lifetime_stats.inserted_samples_for_deceleration +=
      stats.jitter_buffer_emitted_count -
      last_lifetime_stats_.jitter_buffer_emitted_count;
  lifetime_stats.removed_samples_for_acceleration +=
      stats.removed_samples_for_acceleration -
      last_lifetime_stats_.removed_samples_for_acceleration;
  lifetime_stats.silent_concealed_samples +=
      stats.silent_concealed_samples -
      last_lifetime_stats_.silent_concealed_samples;
  lifetime_stats.fec_packets_received +=
      stats.fec_packets_received - last_lifetime_stats_.fec_packets_received;
  lifetime_stats.fec_packets_discarded +=
      stats.fec_packets_discarded - last_lifetime_stats_.fec_packets_discarded;
  lifetime_stats.delayed_packet_outage_samples +=
      stats.delayed_packet_outage_samples -
      last_lifetime_stats_.delayed_packet_outage_samples;
  lifetime_stats.relative_packet_arrival_delay_ms +=
      stats.relative_packet_arrival_delay_ms -
      last_lifetime_stats_.relative_packet_arrival_delay_ms;
  lifetime_stats.jitter_buffer_packets_received +=
      stats.jitter_buffer_packets_received -
      last_lifetime_stats_.jitter_buffer_packets_received;
  lifetime_stats.interruption_count +=
      stats.interruption_count - last_lifetime_stats_.interruption_count;
  lifetime_stats.total_interruption_duration_ms +=
      stats.total_interruption_duration_ms -
      last_lifetime_stats_.total_interruption_duration_ms;

  lifetime_stats_.emplace_back(time_now_ms, lifetime_stats);
}

}  // namespace test
}  // namespace webrtc
