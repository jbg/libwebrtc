/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/cross_media_metrics_reporter.h"

#include <utility>
#include <vector>

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
#include "api/units/timestamp.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void CrossMediaMetricsReporter::Start(
    absl::string_view test_case_name,
    const TrackIdStreamLabelMap* analyzer_helper) {
  test_case_name_ = std::string(test_case_name);
  analyzer_helper_ = analyzer_helper;
}

void CrossMediaMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  auto inbound_stats = report->GetStatsOfType<RTCInboundRTPStreamStats>();
  std::map<absl::string_view, std::vector<const RTCInboundRTPStreamStats*>>
      statsByTimestampAndTrackId;
  for (const auto& stat : inbound_stats) {
    auto media_source_stat =
        report->GetAs<RTCMediaStreamTrackStats>(*stat->track_id);
    if (stat->estimated_playout_timestamp.ValueOrDefault(0.) > 0 &&
        !media_source_stat->track_identifier.ValueOrDefault("").empty()) {
      statsByTimestampAndTrackId[analyzer_helper_->GetSyncGroupLabelFromTrackId(
                                     *media_source_stat->track_identifier)]
          .push_back(stat);
    }
  }

  rtc::CritScope cs(&lock_);
  for (const auto& pair : statsByTimestampAndTrackId) {
    // AV sync metrics can be gathered if there is a sync group of two tracks -
    // video and audio.
    if (pair.second.size() != 2) {
      continue;
    }
    auto sync_group_label = pair.first;
    auto audio_stat = pair.second[0];
    auto video_stat = pair.second[1];
    if (!audio_stat->kind.is_defined() || !video_stat->kind.is_defined() ||
        *audio_stat->kind == *video_stat->kind) {
      continue;
    }
    if (*audio_stat->kind == RTCMediaStreamTrackKind::kVideo) {
      std::swap(audio_stat, video_stat);
    }
    double audio_video_playout_diff = *audio_stat->estimated_playout_timestamp -
                                      *video_stat->estimated_playout_timestamp;
    if (audio_video_playout_diff > 0) {
      stats_[sync_group_label].audio_ahead_ms.AddSample(
          audio_video_playout_diff);
      stats_[sync_group_label].video_ahead_ms.AddSample(0);
    } else {
      stats_[sync_group_label].audio_ahead_ms.AddSample(0);
      stats_[sync_group_label].video_ahead_ms.AddSample(
          std::abs(audio_video_playout_diff));
    }
  }
}

void CrossMediaMetricsReporter::StopAndReportResults() {
  rtc::CritScope cs(&lock_);
  for (const auto& pair : stats_) {
    ReportResult("audio_ahead_ms",
                 GetTestCaseName(/*sync_group_label=*/pair.first),
                 pair.second.audio_ahead_ms, "ms",
                 webrtc::test::ImproveDirection::kSmallerIsBetter);
    ReportResult("video_ahead_ms",
                 GetTestCaseName(/*sync_group_label=*/pair.first),
                 pair.second.video_ahead_ms, "ms",
                 webrtc::test::ImproveDirection::kSmallerIsBetter);
  }
}

void CrossMediaMetricsReporter::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) {
  test::PrintResult(metric_name, /*modifier=*/"", test_case_name, counter, unit,
                    /*important=*/false, improve_direction);
}

std::string CrossMediaMetricsReporter::GetTestCaseName(
    const absl::string_view sync_group_label) const {
  return std::string(test_case_name_) + "/" + std::string(sync_group_label);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
