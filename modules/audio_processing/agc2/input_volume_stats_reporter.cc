/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/input_volume_stats_reporter.h"

#include <cmath>

#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

constexpr int kFramesIn60Seconds = 6000;
constexpr int kMinInputVolume = 0;
constexpr int kMaxInputVolume = 255;
constexpr int kMaxUpdate = kMaxInputVolume - kMinInputVolume;

float ComputeAverageUpdate(int sum_updates, int num_updates) {
  RTC_DCHECK_GE(sum_updates, 0);
  RTC_DCHECK_LE(sum_updates, kMaxUpdate * kFramesIn60Seconds);
  RTC_DCHECK_GE(num_updates, 0);
  RTC_DCHECK_LE(num_updates, kFramesIn60Seconds);
  if (num_updates == 0) {
    return 0.0f;
  }
  return std::round(static_cast<float>(sum_updates) /
                    static_cast<float>(num_updates));
}

metrics::Histogram* CreateDecreaseRateHistogram(bool is_applied_input_volume) {
  return metrics::HistogramFactoryGetCounts(
      /*name=*/is_applied_input_volume
          ? "WebRTC.Audio.Apm.AppliedInputVolume.DecreaseRate"
          : "WebRTC.Audio.Apm.RecommendedInputVolume.DecreaseRate",
      /*min=*/1,
      /*max=*/kFramesIn60Seconds,
      /*bucket_count=*/50);
}

metrics::Histogram* CreateDecreaseAverageHistogram(
    bool is_applied_input_volume) {
  return metrics::HistogramFactoryGetCounts(
      /*name=*/is_applied_input_volume
          ? "WebRTC.Audio.Apm.AppliedInputVolume.DecreaseAverage"
          : "WebRTC.Audio.Apm.RecommendedInputVolume.DecreaseAverage",
      /*min=*/1,
      /*max=*/kMaxUpdate,
      /*bucket_count=*/50);
}

metrics::Histogram* CreateIncreaseRateHistogram(bool is_applied_input_volume) {
  return metrics::HistogramFactoryGetCounts(
      /*name=*/is_applied_input_volume
          ? "WebRTC.Audio.Apm.AppliedInputVolume.IncreaseRate"
          : "WebRTC.Audio.Apm.RecommendedInputVolume.IncreaseRate",
      /*min=*/1,
      /*max=*/kFramesIn60Seconds,
      /*bucket_count=*/50);
}

metrics::Histogram* CreateIncreaseAverageHistogram(
    bool is_applied_input_volume) {
  return metrics::HistogramFactoryGetCounts(
      /*name=*/is_applied_input_volume
          ? "WebRTC.Audio.Apm.AppliedInputVolume.IncreaseAverage"
          : "WebRTC.Audio.Apm.RecommendedInputVolume.IncreaseAverage",
      /*min=*/1,
      /*max=*/kMaxUpdate,
      /*bucket_count=*/50);
}

metrics::Histogram* CreateUpdateRateHistogram(bool is_applied_input_volume) {
  return metrics::HistogramFactoryGetCounts(
      /*name=*/is_applied_input_volume
          ? "WebRTC.Audio.Apm.AppliedInputVolume.UpdateRate"
          : "WebRTC.Audio.Apm.RecommendedInputVolume.UpdateRate",
      /*min=*/1,
      /*max=*/kFramesIn60Seconds,
      /*bucket_count=*/50);
}

metrics::Histogram* CreateUpdateAverageHistogram(bool is_applied_input_volume) {
  return metrics::HistogramFactoryGetCounts(
      /*name=*/is_applied_input_volume
          ? "WebRTC.Audio.Apm.AppliedInputVolume.UpdateAverage"
          : "WebRTC.Audio.Apm.RecommendedInputVolume.UpdateAverage",
      /*min=*/1,
      /*max=*/kMaxUpdate,
      /*bucket_count=*/50);
}

}  // namespace

InputVolumeStatsReporter::InputVolumeStatsReporter(bool is_applied_input_volume)
    : decrease_rate_histogram_(
          CreateDecreaseRateHistogram(is_applied_input_volume)),
      decrease_average_histogram_(
          CreateDecreaseAverageHistogram(is_applied_input_volume)),
      increase_rate_histogram_(
          CreateIncreaseRateHistogram(is_applied_input_volume)),
      increase_average_histogram_(
          CreateIncreaseAverageHistogram(is_applied_input_volume)),
      update_rate_histogram_(
          CreateUpdateRateHistogram(is_applied_input_volume)),
      update_average_histogram_(
          CreateUpdateAverageHistogram(is_applied_input_volume)) {}

InputVolumeStatsReporter::~InputVolumeStatsReporter() = default;

void InputVolumeStatsReporter::UpdateStatistics(int input_volume) {
  RTC_DCHECK_GE(input_volume, kMinInputVolume);
  RTC_DCHECK_LE(input_volume, kMaxInputVolume);
  if (previous_input_volume_.has_value() &&
      input_volume != previous_input_volume_.value()) {
    const int level_change = input_volume - previous_input_volume_.value();
    if (level_change < 0) {
      ++level_update_stats_.num_decreases;
      level_update_stats_.sum_decreases -= level_change;
    } else {
      ++level_update_stats_.num_increases;
      level_update_stats_.sum_increases += level_change;
    }
  }
  // Periodically log input volume change metrics.
  if (++log_level_update_stats_counter_ >= kFramesIn60Seconds) {
    LogLevelUpdateStats();
    level_update_stats_ = {};
    log_level_update_stats_counter_ = 0;
  }
  previous_input_volume_ = input_volume;
}

void InputVolumeStatsReporter::LogLevelUpdateStats() const {
  const float average_decrease = ComputeAverageUpdate(
      level_update_stats_.sum_decreases, level_update_stats_.num_decreases);
  const float average_increase = ComputeAverageUpdate(
      level_update_stats_.sum_increases, level_update_stats_.num_increases);
  const int num_updates =
      level_update_stats_.num_decreases + level_update_stats_.num_increases;
  const float average_update = ComputeAverageUpdate(
      level_update_stats_.sum_decreases + level_update_stats_.sum_increases,
      num_updates);
  metrics::HistogramAdd(decrease_rate_histogram_,
                        level_update_stats_.num_decreases);
  if (level_update_stats_.num_decreases > 0) {
    metrics::HistogramAdd(decrease_average_histogram_, average_decrease);
  }
  metrics::HistogramAdd(increase_rate_histogram_,
                        level_update_stats_.num_increases);
  if (level_update_stats_.num_increases > 0) {
    metrics::HistogramAdd(increase_average_histogram_, average_increase);
  }
  metrics::HistogramAdd(update_rate_histogram_, num_updates);
  if (num_updates > 0) {
    metrics::HistogramAdd(update_average_histogram_, average_update);
  }
}

}  // namespace webrtc
