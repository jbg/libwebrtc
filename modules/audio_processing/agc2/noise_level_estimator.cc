/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/noise_level_estimator.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <numeric>

#include "api/array_view.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
constexpr int kFramesPerSecond = 100;

float FrameEnergy(const AudioFrameView<const float>& audio) {
  float energy = 0.f;
  for (size_t k = 0; k < audio.num_channels(); ++k) {
    float channel_energy =
        std::accumulate(audio.channel(k).begin(), audio.channel(k).end(), 0.f,
                        [](float a, float b) -> float { return a + b * b; });
    energy = std::max(channel_energy, energy);
  }
  return energy;
}

float EnergyToDbfs(float signal_energy, size_t num_samples) {
  const float rms = std::sqrt(signal_energy / num_samples);
  return FloatS16ToDbfs(rms);
}
}  // namespace

NoiseLevelEstimator::NoiseLevelEstimator(ApmDataDumper* data_dumper)
    : data_dumper_(data_dumper), signal_classifier_(data_dumper) {
  Initialize(48000);
}

NoiseLevelEstimator::~NoiseLevelEstimator() {}

void NoiseLevelEstimator::Initialize(int sample_rate_hz) {
  sample_rate_hz_ = sample_rate_hz;
  noise_energy_ = 1.f;
  first_update_ = true;
  min_noise_energy_ = sample_rate_hz * 2.f * 2.f / kFramesPerSecond;
  noise_energy_hold_counter_ = 0;
  signal_classifier_.Initialize(sample_rate_hz);
}

float NoiseLevelEstimator::Analyze(const AudioFrameView<const float>& frame) {
  const int rate =
      static_cast<int>(frame.samples_per_channel() * kFramesPerSecond);
  if (rate != sample_rate_hz_) {
    Initialize(rate);
  }
  const float frame_energy = FrameEnergy(frame);
  if (frame_energy <= 0.f) {
    RTC_DCHECK_GE(frame_energy, 0.f);
    data_dumper_->DumpRaw("agc2_noise_level_estimator_signal_type", -1);
    data_dumper_->DumpRaw("agc2_noise_level_estimator_hold_counter",
                          noise_energy_hold_counter_);
    return EnergyToDbfs(noise_energy_, frame.samples_per_channel());
  }

  if (first_update_) {
    // Initialize the noise energy to the frame energy.
    first_update_ = false;
    data_dumper_->DumpRaw("agc2_noise_level_estimator_signal_type", -1);
    data_dumper_->DumpRaw("agc2_noise_level_estimator_hold_counter",
                          noise_energy_hold_counter_);
    noise_energy_ = std::max(frame_energy, min_noise_energy_);
    return EnergyToDbfs(noise_energy_, frame.samples_per_channel());
  }

  const SignalClassifier::SignalType signal_type =
      signal_classifier_.Analyze(frame.channel(0));
  data_dumper_->DumpRaw("agc2_noise_level_estimator_signal_type",
                        static_cast<int>(signal_type));

  // Update the noise estimate in a minimum statistics-type manner.
  if (signal_type == SignalClassifier::SignalType::kStationary) {
    if (frame_energy > noise_energy_) {
      // Leak the estimate upwards towards the frame energy if no recent
      // downward update.
      noise_energy_hold_counter_ = std::max(noise_energy_hold_counter_ - 1, 0);

      if (noise_energy_hold_counter_ == 0) {
        noise_energy_ = std::min(noise_energy_ * 1.01f, frame_energy);
      }
    } else {
      // Update smoothly downwards with a limited maximum update magnitude.
      constexpr float kMinNoiseEnergyFactor = 0.9f;
      constexpr float kNoiseEnergyDeltaFactor = 0.05f;
      noise_energy_ =
          std::max(noise_energy_ * kMinNoiseEnergyFactor,
                   noise_energy_ - kNoiseEnergyDeltaFactor *
                                       (noise_energy_ - frame_energy));
      // Prevent an energy increase for a period of time.
      constexpr int kTimeToEnergyIncreasedAllowedNumFrames = 200;  // 2 seconds.
      noise_energy_hold_counter_ = kTimeToEnergyIncreasedAllowedNumFrames;
    }
  } else {
    // TODO(bugs.webrtc.org/7494): Remove to not loose the estimated level.
    // For a non-stationary signal, leak the estimate downwards in order to
    // avoid estimate locking due to incorrect signal classification.
    noise_energy_ = noise_energy_ * 0.99f;
  }
  data_dumper_->DumpRaw("agc2_noise_level_estimator_hold_counter",
                        noise_energy_hold_counter_);

  // Ensure a minimum of the estimate.
  noise_energy_ = std::max(noise_energy_, min_noise_energy_);
  return EnergyToDbfs(noise_energy_, frame.samples_per_channel());
}

}  // namespace webrtc
