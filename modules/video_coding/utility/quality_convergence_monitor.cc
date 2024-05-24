/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/quality_convergence_monitor.h"
namespace webrtc {

// static
bool QualityConvergenceMonitor::AtTarget(VideoCodecType codec_type, int qp) {
  if (qp < 0) {
    return false;
  }

  switch (codec_type) {
    case kVideoCodecVP8:
      return qp <= kVp8SteadyStateQpThreshold;
    case kVideoCodecVP9:
      return qp <= kVp9SteadyStateQpThreshold;
    case kVideoCodecAV1:
      return qp <= kAv1SteadyStateQpThreshold;
    case kVideoCodecGeneric:
    case kVideoCodecH264:
    case kVideoCodecH265:
      return false;
  }
  return false;
}

}  // namespace webrtc
