/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_path_gain_analyzer.h"

namespace webrtc {

void EchoPathGainAnalyzer::Update(float echo_path_gain, bool active_render) {
  if (!active_render || consistent_echo_path_gain_) {
    return;
  }
  number_updates_++;
  if (echo_path_gain > 3.0f * echo_path_gain_thr_) {
    next_potential_consistent_update_ =
        number_updates_ +
        kMinimumTimeForConsistencySeconds * kNumBlocksPerSecond;
    echo_path_gain_thr_ = echo_path_gain;
  } else if (echo_path_gain > echo_path_gain_thr_) {
    echo_path_gain_thr_ += 0.001f * (echo_path_gain - echo_path_gain_thr_);
  } else {
    echo_path_gain_thr_ += 0.1f * (echo_path_gain - echo_path_gain_thr_);
  }
  consistent_echo_path_gain_ =
      number_updates_ > next_potential_consistent_update_;
}

}  // namespace webrtc
