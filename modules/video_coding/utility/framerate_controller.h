/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_FRAMERATE_CONTROLLER_H_
#define MODULES_VIDEO_CODING_UTILITY_FRAMERATE_CONTROLLER_H_

#include "rtc_base/rate_statistics.h"

namespace webrtc {

class FramerateController {
 public:
  explicit FramerateController(float target_framerate_fps);

  void SetTargetRate(float target_framerate_fps);

  absl::optional<float> Rate(uint32_t timestamp_ms) const;

  bool DropFrame(uint32_t timestamp_ms) const;

  void AddFrame(uint32_t timestamp_ms);

  void Reset();

 private:
  float target_framerate_fps_;
  uint32_t min_frame_interval_ms_;
  RateStatistics framerate_;
  uint32_t last_timestamp_ms_;
  bool is_last_timestamp_set_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_FRAMERATE_CONTROLLER_H_
