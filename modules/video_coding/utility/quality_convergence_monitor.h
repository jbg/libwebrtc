/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_QUALITY_CONVERGENCE_MONITOR_H_
#define MODULES_VIDEO_CODING_UTILITY_QUALITY_CONVERGENCE_MONITOR_H_

#include "api/video/video_codec_type.h"

namespace webrtc {

// QP levels below which variable framerate and zero hertz screencast reduces
// framerate due to diminishing quality enhancement returns.
constexpr int kVp8SteadyStateQpThreshold = 15;
constexpr int kVp9SteadyStateQpThreshold = 32;
constexpr int kAv1SteadyStateQpThreshold = 40;

class QualityConvergenceMonitor {
 public:
  static bool AtTarget(VideoCodecType codec_type, int qp);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_QUALITY_CONVERGENCE_MONITOR_H_
