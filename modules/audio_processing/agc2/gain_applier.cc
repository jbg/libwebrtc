/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/gain_applier.h"

#include "modules/audio_processing/agc2/agc2_common.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// Returns true when the gain factor is so close to 1 that it would
// not affect int16 samples.
bool GainCloseToOne(float gain_factor) {
  return 1.f - 1.f / kMaxFloatS16Value <= gain_factor &&
         gain_factor <= 1.f + 1.f / kMaxFloatS16Value;
}

void ClipSignal(AudioFrameView<float> signal) {
  for (size_t k = 0; k < signal.num_channels(); ++k) {
    rtc::ArrayView<float> channel_view = signal.channel(k);
    for (auto& sample : channel_view) {
      sample = rtc::SafeClamp(sample, kMinFloatS16Value, kMaxFloatS16Value);
    }
  }
}

}  // namespace

GainApplier::GainApplier(bool hard_clip_samples, float initial_gain_factor)
    : hard_clip_samples_(hard_clip_samples),
      last_gain_factor_(initial_gain_factor),
      current_gain_factor_(initial_gain_factor) {}

void GainApplier::ApplyGain(AudioFrameView<float> signal) {
  // Do not modify the signal when input is loud.
  if (last_gain_factor_ == current_gain_factor_ &&
      GainCloseToOne(current_gain_factor_)) {
    return;
  }

  // A typical case: gain is constant and different from 1.
  if (last_gain_factor_ == current_gain_factor_) {
    for (size_t k = 0; k < signal.num_channels(); ++k) {
      rtc::ArrayView<float> channel_view = signal.channel(k);
      for (auto& sample : channel_view) {
        sample *= current_gain_factor_;
      }
    }
    // Hard clip samples if configured to.
    if (hard_clip_samples_) {
      ClipSignal(signal);
    }
    return;
  }

  // The gain changes. We have to change slowly to avoid discontinuities.
  const size_t samples = signal.samples_per_channel();
  RTC_DCHECK_GT(samples, 0);
  const float increment = (current_gain_factor_ - last_gain_factor_) / samples;
  float gain = last_gain_factor_;
  for (size_t i = 0; i < samples; ++i) {
    for (size_t ch = 0; ch < signal.num_channels(); ++ch) {
      signal.channel(ch)[i] *= gain;
    }
    gain += increment;
  }
  last_gain_factor_ = current_gain_factor_;

  // Hard clip samples if configured to.
  if (hard_clip_samples_) {
    ClipSignal(signal);
  }
}

void GainApplier::SetGainFactor(float gain_factor) {
  RTC_DCHECK_GT(gain_factor, 0.f);
  current_gain_factor_ = gain_factor;
}

}  // namespace webrtc
