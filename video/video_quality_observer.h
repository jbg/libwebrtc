/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_QUALITY_OBSERVER_H_
#define VIDEO_VIDEO_QUALITY_OBSERVER_H_

#include <stdint.h>
#include <vector>

#include "api/optional.h"
#include "api/video/video_content_type.h"
#include "rtc_base/numerics/sample_counter.h"

namespace webrtc {

// Calculates spatial and temporal quality metrics and reports them to UMA
// stats.
class VideoQualityObserver {
 public:
  // Use either VideoQualityObserver::kBlockyQpThresholdVp8 or
  // VideoQualityObserver::kBlockyQpThresholdVp9.
  VideoQualityObserver();
  ~VideoQualityObserver();

  void OnDecodedFrame(rtc::Optional<uint8_t> qp,
                      int width,
                      int height,
                      VideoContentType content_type,
                      int64_t now_ms,
                      bool is_vp9);

 private:
  void UpdateHistograms();

  enum Resolution {
    Low = 0,
    Medium = 1,
    High = 2,
  };
  static const int kMinFrameSamplesToDetectFreeze = 5;
  static const int kMinCallDurationMs = 3000;
  static const int kMinRequiredSamples = 1;
  static const int kMinIncreaseForFreezeMs = 150;
  static const int kPixelsInHighResolution = 1280 * 720;
  static const int kPixelsInMediumResolution = 640 * 360;
  static const int kBlockyQpThresholdVp8 = 70;
  static const int kBlockyQpThresholdVp9 = 60;  // TODO(ilnik): tune this value.
  int64_t last_frame_decoded_ms_;
  int64_t num_frames_decoded_;
  int64_t first_frame_decoded_ms_;
  int64_t last_frame_pixels_;
  uint8_t last_frame_qp_;
  int64_t last_unfreeze_time_;
  rtc::SampleCounter interframe_delays_;
  rtc::SampleCounter freezes_durations_;
  rtc::SampleCounter smooth_playback_durations_;
  std::vector<int64_t> time_in_resolution_ms_;
  Resolution current_resolution_;
  int num_resolution_downgrades_;
  int64_t time_in_blocky_video_ms_;
  VideoContentType last_content_type_;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_QUALITY_OBSERVER_H_
