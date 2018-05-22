/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_quality_observer.h"

#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

VideoQualityObserver::VideoQualityObserver()
    : last_frame_decoded_ms_(-1),
      num_frames_decoded_(0),
      first_frame_decoded_ms_(-1),
      last_frame_pixels_(0),
      last_frame_qp_(0),
      last_unfreeze_time_(0),
      time_in_resolution_ms_(3, 0),
      current_resolution_(Resolution::Low),
      num_resolution_downgrades_(0),
      time_in_blocky_video_ms_(0),
      last_content_type_(VideoContentType::UNSPECIFIED) {}

VideoQualityObserver::~VideoQualityObserver() {
  UpdateHistograms();
}

void VideoQualityObserver::UpdateHistograms() {
  // Don't report anything on an empty video stream.
  if (num_frames_decoded_ == 0) {
    return;
  }

  char log_stream_buf[2 * 1024];
  rtc::SimpleStringBuilder log_stream(log_stream_buf);

  if (last_frame_decoded_ms_ > last_unfreeze_time_) {
    smooth_playback_durations_.Add(last_frame_decoded_ms_ -
                                   last_unfreeze_time_);
  }
  int64_t call_duration_ms = last_frame_decoded_ms_ - first_frame_decoded_ms_;

  int time_spent_in_hd_percentage = static_cast<int>(
      time_in_resolution_ms_[Resolution::High] * 100 / call_duration_ms);
  int time_with_blocky_video_percentage =
      static_cast<int>(time_in_blocky_video_ms_ * 100 / call_duration_ms);

  std::string uma_prefix =
      videocontenttypehelpers::IsScreenshare(last_content_type_)
          ? "WebRTC.Video.Screenshare"
          : "WebRTC.Video";

  auto mean_time_between_freezes =
      smooth_playback_durations_.Avg(kMinRequiredSamples);
  if (mean_time_between_freezes) {
    RTC_HISTOGRAM_COUNTS_SPARSE_10000(uma_prefix + ".MeanTimeBetweenFreezesMs",
                                      *mean_time_between_freezes);
    log_stream << uma_prefix << ".MeanTimeBetweenFreezesMs "
               << *mean_time_between_freezes << "\n";
  }
  auto avg_freeze_length = freezes_durations_.Avg(kMinRequiredSamples);
  if (avg_freeze_length) {
    RTC_HISTOGRAM_COUNTS_SPARSE_10000(uma_prefix + ".MeanFreezeDurationMs",
                                      *avg_freeze_length);
    log_stream << uma_prefix << ".MeanFreezeDurationMs " << *avg_freeze_length
               << "\n";
  }
  if (call_duration_ms >= kMinCallDurationMs) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100(uma_prefix + ".TimeInHdPercentage",
                                    time_spent_in_hd_percentage);
    log_stream << uma_prefix << ".TimeInHdPercentage "
               << time_spent_in_hd_percentage << "\n";
    RTC_HISTOGRAM_COUNTS_SPARSE_100(uma_prefix + ".TimeInBlockyVideoPercentage",
                                    time_with_blocky_video_percentage);
    log_stream << uma_prefix << ".TimeInBlockyVideoPercentage "
               << time_with_blocky_video_percentage << "\n";
  }
  RTC_HISTOGRAM_COUNTS_SPARSE_1000(uma_prefix + ".NumberResolutionDownswitches",
                                   num_resolution_downgrades_);
  log_stream << uma_prefix << ".NumberResolutionDownswitches "
             << num_resolution_downgrades_ << "\n";
  RTC_LOG(LS_INFO) << log_stream.str();
}

void VideoQualityObserver::OnDecodedFrame(rtc::Optional<uint8_t> qp,
                                          int width,
                                          int height,
                                          VideoContentType content_type,
                                          int64_t now_ms,
                                          bool is_vp9) {
  if (num_frames_decoded_ > 0 &&
      videocontenttypehelpers::IsScreenshare(content_type) !=
          videocontenttypehelpers::IsScreenshare(last_content_type_)) {
    // To count video and screenshare separately, treat as if stream terminated.
    // Explicitly call destructor which will trigger UpdateHistograms().
    this->~VideoQualityObserver();
    // In-place constructor to reinitialize everything.
    new (this) VideoQualityObserver();
    // Continue execution below as if this is the first call to
    // OnDecodedFrame().
  }

  if (num_frames_decoded_ == 0) {
    first_frame_decoded_ms_ = now_ms;
    last_unfreeze_time_ = now_ms;
    last_content_type_ = content_type;
  }

  ++num_frames_decoded_;
  if (num_frames_decoded_ > 1) {
    int64_t interframe_delay_ms = now_ms - last_frame_decoded_ms_;
    interframe_delays_.Add(interframe_delay_ms);
    if (num_frames_decoded_ > kMinFrameSamplesToDetectFreeze &&
        interframe_delay_ms >=
            *interframe_delays_.Avg(kMinFrameSamplesToDetectFreeze) +
                kMinIncreaseForFreezeMs) {
      freezes_durations_.Add(interframe_delay_ms);
      smooth_playback_durations_.Add(last_frame_decoded_ms_ -
                                     last_unfreeze_time_);
      last_unfreeze_time_ = now_ms;
    } else {
      // Only count interframe delay as playback time if there
      // was no freeze.
      time_in_resolution_ms_[current_resolution_] += interframe_delay_ms;
      if (qp.value_or(0) >
          (is_vp9 ? kBlockyQpThresholdVp9 : kBlockyQpThresholdVp8)) {
        time_in_blocky_video_ms_ += interframe_delay_ms;
      }
    }
  }

  int64_t pixels = width * height;
  if (pixels >= kPixelsInHighResolution) {
    current_resolution_ = Resolution::High;
  } else if (pixels >= kPixelsInMediumResolution) {
    current_resolution_ = Resolution::Medium;
  } else {
    current_resolution_ = Resolution::Low;
  }

  if (pixels < last_frame_pixels_) {
    ++num_resolution_downgrades_;
  }

  last_frame_decoded_ms_ = now_ms;
  last_frame_qp_ = qp.value_or(0);
  last_frame_pixels_ = pixels;
}
}  // namespace webrtc
