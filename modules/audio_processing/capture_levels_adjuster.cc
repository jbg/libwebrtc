/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_levels_adjuster.h"

#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {

float ComputeLevelBasedGain(int pre_gain_level) {
  constexpr int kMinGainLevel = 0;
  constexpr int kMaxGainLevel = 255;
  static_assert(
      kMinGainLevel == 0,
      "The minimum gain level must be 0 for the maths below to work.");
  constexpr float kGainToLevelMultiplier = 1.f / kMaxGainLevel;

  RTC_DCHECK_GE(pre_gain_level, kMinGainLevel);
  RTC_DCHECK_LE(pre_gain_level, kMaxGainLevel);
  int clamped_level =
      rtc::SafeClamp(pre_gain_level, kMinGainLevel, kMaxGainLevel);
  return kGainToLevelMultiplier * clamped_level;
}

float ComputePreGain(float pre_gain, int pre_gain_level) {
  return pre_gain * ComputeLevelBasedGain(pre_gain_level);
}

}  // namespace

CaptureLevelsAdjuster::CaptureLevelsAdjuster(int pre_gain_level,
                                             float pre_gain,
                                             float post_gain)
    : pre_gain_level_(pre_gain_level),
      pre_gain_(pre_gain),
      pre_scaler_(ComputePreGain(pre_gain_, pre_gain_level_)),
      post_scaler_(post_gain) {}

void CaptureLevelsAdjuster::PreLevelAdjustment(AudioBuffer& audio_buffer) {
  pre_scaler_.Process(audio_buffer);
}

void CaptureLevelsAdjuster::PostLevelAdjustment(AudioBuffer& audio_buffer) {
  post_scaler_.Process(audio_buffer);
}

void CaptureLevelsAdjuster::SetPreGain(float pre_gain) {
  pre_gain_ = pre_gain;
  pre_scaler_.SetGain(ComputePreGain(pre_gain_, pre_gain_level_));
}

void CaptureLevelsAdjuster::SetPostGain(float post_gain) {
  post_scaler_.SetGain(post_gain);
}

void CaptureLevelsAdjuster::SetPreGainLevel(int pre_gain_level) {
  pre_gain_level_ = pre_gain_level;
  pre_scaler_.SetGain(ComputePreGain(pre_gain_, pre_gain_level_));
}

}  // namespace webrtc
