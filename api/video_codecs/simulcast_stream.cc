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

namespace webrtc {

unsigned char SimulcastStream::GetNumberOfTemporalLayers() const {
  return ScalabilityModeToNumTemporalLayers(scalability_mode);
}

ScalabilityMode SimulcastStream::GetScalabilityMode() const {
  return scalability_mode;
}

}  // namespace webrtc
