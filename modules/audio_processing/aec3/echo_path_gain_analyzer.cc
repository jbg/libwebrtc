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

#include <algorithm>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

namespace {
constexpr float kMinimumTimeForConsistencySeconds = 1.0f;
constexpr float kTimeOutSeconds = 30.0f;
constexpr float kMinimumEchoPathGain = 0.0001f;
constexpr float kSecondsStartDetectingLowEchoPathGain = 2.0f;

}  // namespace

EchoPathGainAnalyzer::EchoPathGainAnalyzer(bool initial_consistent_status)
    : consistent_echo_path_gain_(initial_consistent_status),
      next_potential_consistent_update_(kMinimumTimeForConsistencySeconds *
                                        kNumBlocksPerSecond) {}

void EchoPathGainAnalyzer::Update(
    rtc::ArrayView<const std::vector<float>> filters_time_domain,
    rtc::ArrayView<const int> filter_delay_blocks,
    bool active_render) {
  if (timeout_) {
    return;
  }
  number_updates_++;
  timeout_ = number_updates_ > kTimeOutSeconds * kNumBlocksPerSecond;
  if (!active_render || consistent_echo_path_gain_) {
    return;
  }
  float echo_path_gain = 0.0f;
  for (size_t ch = 0; ch < filters_time_domain.size(); ++ch) {
    rtc::ArrayView<const float> h(filters_time_domain[ch]);
    int start_idx = filter_delay_blocks[ch] << kBlockSizeLog2;
    int stop_idx = std::min(start_idx + static_cast<int>(kBlockSize),
                            static_cast<int>(h.size()));
    for (int k = start_idx; k < stop_idx; ++k) {
      echo_path_gain += h[k] * h[k];
    }
  }
  number_render_updates_++;
  if (echo_path_gain > 10.0f * echo_path_gain_thr_) {
    next_potential_consistent_update_ =
        number_render_updates_ +
        kMinimumTimeForConsistencySeconds * kNumBlocksPerSecond;
    echo_path_gain_thr_ = echo_path_gain;
  } else if (echo_path_gain > echo_path_gain_thr_) {
    echo_path_gain_thr_ += 0.001f * (echo_path_gain - echo_path_gain_thr_);
  } else {
    echo_path_gain_thr_ += 0.1f * (echo_path_gain - echo_path_gain_thr_);
  }
  consistent_echo_path_gain_ =
      number_render_updates_ > next_potential_consistent_update_;
  low_echo_path_gain_ =
      number_render_updates_ >
          kSecondsStartDetectingLowEchoPathGain * kNumBlocksPerSecond &&
      echo_path_gain_thr_ < kMinimumEchoPathGain;
}

}  // namespace webrtc
