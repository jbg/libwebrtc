/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/framerate_controller.h"

#include "rtc_base/checks.h"

namespace webrtc {

FramerateController::FramerateController(float target_framerate_fps)
    : target_framerate_fps_(0),
      is_target_framerate_set_(false),
      last_timestamp_ms_(0),
      is_last_timestamp_set_(false),
      min_frame_interval_ms_(0),
      framerate_estimator_(1000.0, 1000.0) {
  SetTargetRate(target_framerate_fps);
}

void FramerateController::SetTargetRate(float target_framerate_fps) {
  if (!is_target_framerate_set_ ||
      target_framerate_fps != target_framerate_fps_) {
    if (is_last_timestamp_set_) {
      framerate_estimator_.Reset();
      framerate_estimator_.Update(1, last_timestamp_ms_);
    }

    const size_t target_frame_interval_ms = 1000 / target_framerate_fps;
    target_framerate_fps_ = target_framerate_fps;
    min_frame_interval_ms_ = 85 * target_frame_interval_ms / 100;

    is_target_framerate_set_ = true;
  }
}

void FramerateController::Reset() {
  framerate_estimator_.Reset();
  is_last_timestamp_set_ = false;
}

absl::optional<float> FramerateController::Rate(uint32_t timestamp_ms) const {
  return framerate_estimator_.Rate(timestamp_ms);
}

bool FramerateController::DropFrame(uint32_t timestamp_ms) const {
  RTC_DCHECK(is_target_framerate_set_);

  if (timestamp_ms < last_timestamp_ms_) {
    // Timestamp jumps backward. We can't make adequate drop decision. Don't
    // drop this frame. Stats will be reset in AddFrame().
    return false;
  }

  if (Rate(timestamp_ms).value_or(target_framerate_fps_) >
      target_framerate_fps_) {
    return true;
  }

  if (is_last_timestamp_set_) {
    const int64_t diff_ms =
        static_cast<int64_t>(timestamp_ms) - last_timestamp_ms_;
    if (diff_ms < min_frame_interval_ms_) {
      return true;
    }
  }

  return false;
}

void FramerateController::AddFrame(uint32_t timestamp_ms) {
  RTC_DCHECK(is_target_framerate_set_);

  if (timestamp_ms < last_timestamp_ms_) {
    // Timestamp jumps backward.
    Reset();
  }

  framerate_estimator_.Update(1, timestamp_ms);
  last_timestamp_ms_ = timestamp_ms;

  if (!is_last_timestamp_set_) {
    is_last_timestamp_set_ = true;
  }
}

}  // namespace webrtc
