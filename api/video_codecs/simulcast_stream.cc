/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/simulcast_stream.h"

#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/checks.h"

namespace webrtc {

unsigned char SimulcastStream::GetNumberOfTemporalLayers() const {
  return ScalabilityModeToNumTemporalLayers(scalability_mode);
}
void SimulcastStream::SetNumberOfTemporalLayers(unsigned char n) {
  RTC_CHECK_GE(n, 1);
  RTC_CHECK_LE(n, 3);
  static const ScalabilityMode scalability_modes[3] = {
      ScalabilityMode::kL1T1,
      ScalabilityMode::kL1T2,
      ScalabilityMode::kL1T3,
  };
  scalability_mode = scalability_modes[n - 1];
}

ScalabilityMode SimulcastStream::GetScalabilityMode() const {
  return scalability_mode;
}

}  // namespace webrtc
