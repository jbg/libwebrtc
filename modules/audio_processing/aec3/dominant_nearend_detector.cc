/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/dominant_nearend_detector.h"

#include <numeric>

#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
class DominantNearendDetectorLegacy : public DominantNearendDetector {
 public:
  DominantNearendDetectorLegacy(
      const EchoCanceller3Config::Suppressor::DominantNearendDetection config);

  // Returns whether the current state is the nearend state.
  bool IsNearendState() const override;

  // Updates the state selection based on latest spectral estimates.
  void Update(rtc::ArrayView<const float> nearend_spectrum,
              rtc::ArrayView<const float> residual_echo_spectrum,
              rtc::ArrayView<const float> comfort_noise_spectrum,
              bool initial_state) override;

 private:
  const float enr_threshold_;
  const float enr_exit_threshold_;
  const float snr_threshold_;
  const int hold_duration_;
  const int trigger_threshold_;
  const bool use_during_initial_phase_;

  bool nearend_state_ = false;
  int trigger_counter_ = 0;
  int hold_counter_ = 0;
};

DominantNearendDetectorLegacy::DominantNearendDetectorLegacy(
    const EchoCanceller3Config::Suppressor::DominantNearendDetection config)
    : enr_threshold_(config.enr_threshold),
      enr_exit_threshold_(config.enr_exit_threshold),
      snr_threshold_(config.snr_threshold),
      hold_duration_(config.hold_duration),
      trigger_threshold_(config.trigger_threshold),
      use_during_initial_phase_(config.use_during_initial_phase) {}

bool DominantNearendDetectorLegacy::IsNearendState() const {
  return nearend_state_;
}

void DominantNearendDetectorLegacy::Update(
    rtc::ArrayView<const float> nearend_spectrum,
    rtc::ArrayView<const float> residual_echo_spectrum,
    rtc::ArrayView<const float> comfort_noise_spectrum,
    bool initial_state) {
  auto low_frequency_energy = [](rtc::ArrayView<const float> spectrum) {
    RTC_DCHECK_LE(16, spectrum.size());
    return std::accumulate(spectrum.begin() + 1, spectrum.begin() + 16, 0.f);
  };
  const float ne_sum = low_frequency_energy(nearend_spectrum);
  const float echo_sum = low_frequency_energy(residual_echo_spectrum);
  const float noise_sum = low_frequency_energy(comfort_noise_spectrum);

  // Detect strong active nearend if the nearend is sufficiently stronger than
  // the echo and the nearend noise.
  if ((!initial_state || use_during_initial_phase_) &&
      echo_sum < enr_threshold_ * ne_sum &&
      ne_sum > snr_threshold_ * noise_sum) {
    if (++trigger_counter_ >= trigger_threshold_) {
      // After a period of strong active nearend activity, flag nearend mode.
      hold_counter_ = hold_duration_;
      trigger_counter_ = trigger_threshold_;
    }
  } else {
    // Forget previously detected strong active nearend activity.
    trigger_counter_ = std::max(0, trigger_counter_ - 1);
  }

  // Exit nearend-state early at strong echo.
  if (echo_sum > enr_exit_threshold_ * ne_sum &&
      echo_sum > snr_threshold_ * noise_sum) {
    hold_counter_ = 0;
  }

  // Remain in any nearend mode for a certain duration.
  hold_counter_ = std::max(0, hold_counter_ - 1);
  nearend_state_ = hold_counter_ > 0;
}

namespace {
// State transition probability normal to nearend.
constexpr float kPNormalToNearend = 0.001f;
// State transition probability nearend to nornal.
constexpr float kPNearendToNormal = 0.001f;
// Probability of observing low ENR and high SNR in normal state.
constexpr float kOutputNormal = 0.1f;
// Probability of observing low ENR and high SNR in nearend state.
constexpr float kOutputNearend = 0.3f;

constexpr float kTransitionProbability[2][2] = {
    {1.f - kPNormalToNearend, kPNormalToNearend},
    {kPNearendToNormal, 1.f - kPNearendToNormal}};

constexpr float kOutputProbability[2][2] = {
    {1 - kOutputNormal, kOutputNormal},
    {1 - kOutputNearend, kOutputNearend}};
}  // namespace

class DominantNearendDetectorHmm : public DominantNearendDetector {
 public:
  DominantNearendDetectorHmm(
      const EchoCanceller3Config::Suppressor::DominantNearendDetection config);

  // Returns whether the current state is the nearend state.
  bool IsNearendState() const override;

  // Updates the state selection based on latest spectral estimates.
  void Update(rtc::ArrayView<const float> nearend_spectrum,
              rtc::ArrayView<const float> residual_echo_spectrum,
              rtc::ArrayView<const float> comfort_noise_spectrum,
              bool initial_state) override;

 private:
  const float enr_threshold_;
  const float snr_threshold_;
  const bool use_during_initial_phase_;

  float p_nearend_ = 0.f;
};

DominantNearendDetectorHmm::DominantNearendDetectorHmm(
    const EchoCanceller3Config::Suppressor::DominantNearendDetection config)
    : enr_threshold_(config.enr_threshold),
      snr_threshold_(config.snr_threshold),
      use_during_initial_phase_(config.use_during_initial_phase) {}

bool DominantNearendDetectorHmm::IsNearendState() const {
  return p_nearend_ >= .5f;
}

void DominantNearendDetectorHmm::Update(
    rtc::ArrayView<const float> nearend_spectrum,
    rtc::ArrayView<const float> residual_echo_spectrum,
    rtc::ArrayView<const float> comfort_noise_spectrum,
    bool initial_state) {
  auto low_frequency_energy = [](rtc::ArrayView<const float> spectrum) {
    RTC_DCHECK_LE(16, spectrum.size());
    return std::accumulate(spectrum.begin() + 1, spectrum.begin() + 16, 0.f);
  };
  const float ne_sum = low_frequency_energy(nearend_spectrum);
  const float echo_sum = low_frequency_energy(residual_echo_spectrum);
  const float noise_sum = low_frequency_energy(comfort_noise_spectrum);

  // Observed output.
  // 1 if low ENR and high SNR, 0 otherwise.
  bool output = (!initial_state || use_during_initial_phase_) &&
                echo_sum < enr_threshold_ * ne_sum &&
                ne_sum > snr_threshold_ * noise_sum;

  // Probability of transitioning to nearend state.
  p_nearend_ = (1.f - p_nearend_) * kTransitionProbability[0][1] +
               p_nearend_ * kTransitionProbability[1][1];

  // Joint probabilities of the output and respective states.
  float p_output_normal = (1.f - p_nearend_) * kOutputProbability[0][output];
  float p_output_nearend = p_nearend_ * kOutputProbability[1][output];

  // Conditional probability of nearend state and the output.
  p_nearend_ = p_output_nearend / (p_output_normal + p_output_nearend);
}

DominantNearendDetector* DominantNearendDetector::Create(
    const EchoCanceller3Config::Suppressor::DominantNearendDetection& config) {
  if (config.use_hmm &&
      !field_trial::IsEnabled(
          "WebRTC-Aec3DominantNearendDetectorHmmKillSwitch")) {
    return new DominantNearendDetectorHmm(config);
  } else {
    return new DominantNearendDetectorLegacy(config);
  }
}
}  // namespace webrtc
