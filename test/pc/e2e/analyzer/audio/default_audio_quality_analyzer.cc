/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"

#include "api/stats/rtcstats_objects.h"
#include "api/stats_types.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

static const char kStatsAudioMediaType[] = "audio";

}  // namespace

void DefaultAudioQualityAnalyzer::Start(
    std::string test_case_name,
    TrackIdStreamLabelMap* analyzer_helper) {
  test_case_name_ = std::move(test_case_name);
  analyzer_helper_ = analyzer_helper;
  start_time_ = Now();
}

void DefaultAudioQualityAnalyzer::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& stats_reports) {
  for (const StatsReport* stats_report : stats_reports) {
    // NetEq stats are only present in kStatsReportTypeSsrc reports, so all
    // other reports are just ignored.
    if (stats_report->type() != StatsReport::StatsType::kStatsReportTypeSsrc) {
      continue;
    }
    // Ignoring stats reports of "video" SSRC.
    const webrtc::StatsReport::Value* media_type = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameMediaType);
    RTC_CHECK(media_type);
    if (strcmp(media_type->static_string_val(), kStatsAudioMediaType) != 0) {
      continue;
    }
    if (stats_report->FindValue(
            webrtc::StatsReport::kStatsValueNameBytesSent)) {
      // If kStatsValueNameBytesSent is present, it means it's a send stream,
      // but we need audio metrics for receive stream, so skip it.
      continue;
    }

    const webrtc::StatsReport::Value* expand_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameExpandRate);
    const webrtc::StatsReport::Value* accelerate_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameAccelerateRate);
    const webrtc::StatsReport::Value* preemptive_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNamePreemptiveExpandRate);
    const webrtc::StatsReport::Value* speech_expand_rate =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameSpeechExpandRate);
    const webrtc::StatsReport::Value* preferred_buffer_size_ms =
        stats_report->FindValue(StatsReport::StatsValueName::
                                    kStatsValueNamePreferredJitterBufferMs);
    RTC_CHECK(expand_rate);
    RTC_CHECK(accelerate_rate);
    RTC_CHECK(preemptive_rate);
    RTC_CHECK(speech_expand_rate);
    RTC_CHECK(preferred_buffer_size_ms);

    const std::string& stream_label =
        GetStreamLabelFromStatsReport(stats_report);

    rtc::CritScope crit(&lock_);
    AudioStreamStats& audio_stream_stats = streams_stats_[stream_label];
    audio_stream_stats.expand_rate.AddSample(expand_rate->float_val());
    audio_stream_stats.accelerate_rate.AddSample(accelerate_rate->float_val());
    audio_stream_stats.preemptive_rate.AddSample(preemptive_rate->float_val());
    audio_stream_stats.speech_expand_rate.AddSample(
        speech_expand_rate->float_val());
    audio_stream_stats.preferred_buffer_size_ms.AddSample(
        preferred_buffer_size_ms->int_val());
  }
}

// TODO(landrey):
// The bottom line: please only use "outbound-rtp" metrics for sending stats,
// not "track". For receiving stats, do use "track" stats in case the metrics
// are not in "inbound-rtp" yet but add a TODO to use "inbound-rtp" instead when
// we have completed the move
void DefaultAudioQualityAnalyzer::OnStatsReports(
    const std::string& pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  RTC_CHECK(start_time_)
      << "Please invoke Start(...) method before calling OnStatsReports(...)";

  auto stats = report->GetStatsOfType<RTCMediaStreamTrackStats>();

  StatsSample sample;
  for (auto& stat : stats) {
    if (!stat->kind.is_defined() ||
        !(*stat->kind == RTCMediaStreamTrackKind::kAudio) ||
        !*stat->remote_source) {
      continue;
    }
    RTC_CHECK(sample.IsEmpty())
        << "There can be only one audio receiving track.";

    if (stat->total_samples_received.is_defined()) {
      sample.total_samples_received = *stat->total_samples_received;
    }
    if (stat->concealed_samples.is_defined()) {
      sample.concealed_samples = *stat->concealed_samples;
    }
    if (stat->removed_samples_for_acceleration.is_defined()) {
      sample.removed_samples_for_acceleration =
          *stat->removed_samples_for_acceleration;
    }
    if (stat->inserted_samples_for_deceleration.is_defined()) {
      sample.inserted_samples_for_deceleration =
          *stat->inserted_samples_for_deceleration;
    }
    if (stat->silent_concealed_samples.is_defined()) {
      sample.silent_concealed_samples = *stat->silent_concealed_samples;
    }
    if (stat->jitter_buffer_target_delay.is_defined()) {
      sample.jitter_buffer_target_delay = *stat->jitter_buffer_target_delay;
    }
    if (stat->jitter_buffer_emitted_count.is_defined()) {
      sample.jitter_buffer_emitted_count = *stat->jitter_buffer_emitted_count;
    }
    sample.sample_time_us = stat->timestamp_us();

    const std::string& stream_label =
        analyzer_helper_->GetStreamLabelFromTrackId(*stat->track_identifier);

    rtc::CritScope crit(&lock_);
    StatsSample prev_sample = last_stats_sample_[stream_label];
    if (prev_sample.IsEmpty()) {
      prev_sample.sample_time_us = start_time_->us();
    }

    // std::printf("Debug log (%s): \n", stream_label.c_str());
    // prev_sample.DebugLog();
    // sample.DebugLog();

    int64_t time_between_samples_us =
        sample.sample_time_us - prev_sample.sample_time_us;
    // double time_between_samples_sec = time_between_samples_us / 1000000.0;
    double total_samples_diff = static_cast<double>(
        sample.total_samples_received - prev_sample.total_samples_received);

    // std::printf("Start time: %lu; current time: %lu;\n", start_time_->us(),
    //            clock_->TimeInMicroseconds());
    // std::printf("Time: %lu; Delta sec: %lf\n", sample.sample_time_us,
    //            time_between_samples_sec);

    if (time_between_samples_us == 0 || total_samples_diff == 0) {
      last_stats_sample_[stream_label] = sample;
      return;
    }

    // std::printf("Calculating\n");

    AudioStreamStats& audio_stream_stats = streams_stats_[stream_label];
    audio_stream_stats.expand_rate_new.AddSample(
        (sample.concealed_samples - prev_sample.concealed_samples) /
        total_samples_diff);
    audio_stream_stats.accelerate_rate_new.AddSample(
        (sample.removed_samples_for_acceleration -
         prev_sample.removed_samples_for_acceleration) /
        total_samples_diff);
    audio_stream_stats.preemptive_rate_new.AddSample(
        (sample.inserted_samples_for_deceleration -
         prev_sample.inserted_samples_for_deceleration) /
        total_samples_diff);

    int64_t concealed_samples_diff =
        sample.concealed_samples - prev_sample.concealed_samples;
    if (concealed_samples_diff > 0) {
      int64_t speech_concealed_samples =
          sample.concealed_samples - sample.silent_concealed_samples;
      int64_t prev_speech_concealed_samples =
          prev_sample.concealed_samples - prev_sample.silent_concealed_samples;
      audio_stream_stats.speech_expand_rate_new.AddSample(
          (speech_concealed_samples - prev_speech_concealed_samples) /
          static_cast<double>(/*total_samples_diff*/ concealed_samples_diff));
    }
    int64_t jitter_buffer_emitted_count_diff =
        sample.jitter_buffer_emitted_count -
        prev_sample.jitter_buffer_emitted_count;
    if (jitter_buffer_emitted_count_diff > 0) {
      double jitter_buffer_target_delay_diff =
          sample.jitter_buffer_target_delay -
          prev_sample.jitter_buffer_target_delay;
      audio_stream_stats.preferred_buffer_size_ms_new.AddSample(
          jitter_buffer_target_delay_diff * 1000.0 /
          jitter_buffer_emitted_count_diff);
    }

    last_stats_sample_[stream_label] = sample;
  }
}

const std::string& DefaultAudioQualityAnalyzer::GetStreamLabelFromStatsReport(
    const StatsReport* stats_report) const {
  const webrtc::StatsReport::Value* report_track_id = stats_report->FindValue(
      StatsReport::StatsValueName::kStatsValueNameTrackId);
  RTC_CHECK(report_track_id);
  return analyzer_helper_->GetStreamLabelFromTrackId(
      report_track_id->string_val());
}

std::string DefaultAudioQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_case_name_ + "/" + stream_label;
}

void DefaultAudioQualityAnalyzer::Stop() {
  using ::webrtc::test::ImproveDirection;
  rtc::CritScope crit(&lock_);
  for (auto& item : streams_stats_) {
    //ReportResult("expand_rate", item.first, item.second.expand_rate, "unitless",
    //             ImproveDirection::kSmallerIsBetter);
    ReportResult("expand_rate", item.first, item.second.expand_rate_new,
                 "unitless", ImproveDirection::kSmallerIsBetter);
    //ReportResult("accelerate_rate", item.first, item.second.accelerate_rate,
    //             "unitless", ImproveDirection::kSmallerIsBetter);
    ReportResult("accelerate_rate", item.first,
                 item.second.accelerate_rate_new, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    //ReportResult("preemptive_rate", item.first, item.second.preemptive_rate,
    //             "unitless", ImproveDirection::kSmallerIsBetter);
    ReportResult("preemptive_rate", item.first,
                 item.second.preemptive_rate_new, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    //ReportResult("speech_expand_rate", item.first,
    //             item.second.speech_expand_rate, "unitless",
    //             ImproveDirection::kSmallerIsBetter);
    ReportResult("speech_expand_rate", item.first,
                 item.second.speech_expand_rate_new, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    //ReportResult("preferred_buffer_size_ms", item.first,
    //             item.second.preferred_buffer_size_ms, "ms",
    //             ImproveDirection::kNone);
    ReportResult("preferred_buffer_size_ms", item.first,
                 item.second.preferred_buffer_size_ms_new, "ms",
                 ImproveDirection::kNone);
  }
}

std::map<std::string, AudioStreamStats>
DefaultAudioQualityAnalyzer::GetAudioStreamsStats() const {
  rtc::CritScope crit(&lock_);
  return streams_stats_;
}

void DefaultAudioQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& stream_label,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) const {
  test::PrintResultMeanAndError(
      metric_name, /*modifier=*/"", GetTestCaseName(stream_label),
      counter.IsEmpty() ? 0 : counter.GetAverage(),
      counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), unit,
      /*important=*/false, improve_direction);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
