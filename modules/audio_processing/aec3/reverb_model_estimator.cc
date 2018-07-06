/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/reverb_model_estimator.h"

#include <algorithm>
#include <array>
#include <memory>
#include <numeric>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

bool EnableSmoothUpdatesTailFreqResp() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3SmoothUpdatesTailFreqRespKillSwitch");
}

// Computes the ratio of the energies between the direct path and the tail. The
// energy is computed in the power spectrum domain discarding the DC
// contributions.
float ComputeRatioEnergies(
    const rtc::ArrayView<const float>& freq_resp_direct_path,
    const rtc::ArrayView<const float>& freq_resp_tail) {
  // Skipping the DC for the ratio computation
  constexpr size_t n_skip_bins = 1;
  RTC_CHECK_EQ(freq_resp_direct_path.size(), freq_resp_tail.size());

  float direct_path_energy =
      std::accumulate(freq_resp_direct_path.begin() + n_skip_bins,
                      freq_resp_direct_path.end(), 0.f);

  float tail_energy = std::accumulate(freq_resp_tail.begin() + n_skip_bins,
                                      freq_resp_tail.end(), 0.f);

  if (direct_path_energy > 0) {
    return tail_energy / direct_path_energy;
  } else {
    return 0.f;
  }
}

}  // namespace

ReverbModelEstimator::ReverbModelEstimator(const EchoCanceller3Config& config)
    : filter_main_length_blocks_(config.filter.main.length_blocks),
      linear_regressor_sections_(config.filter.main.length_blocks -
                                 kBlocksFirstReflections),
      reverb_decay_(fabsf(config.ep_strength.default_len)),
      enable_smooth_freq_resp_tail_updates_(EnableSmoothUpdatesTailFreqResp()) {
  block_energies_.fill(0.f);
  freq_resp_tail_.fill(0.f);
}

ReverbModelEstimator::~ReverbModelEstimator() = default;

bool ReverbModelEstimator::IsAGoodFilterForDecayEstimation(
    int filter_delay_blocks,
    bool usable_linear_estimate,
    size_t length_filter) {
  if ((filter_delay_blocks && usable_linear_estimate) &&
      (filter_delay_blocks <=
       static_cast<int>(filter_main_length_blocks_) - 4) &&
      (length_filter >=
       static_cast<size_t>(GetTimeDomainLength(filter_main_length_blocks_)))) {
    return true;
  } else {
    return false;
  }
}

void ReverbModelEstimator::Update(
    const std::vector<float>& impulse_response,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_freq_response,
    const absl::optional<float>& quality_linear,
    int filter_delay_blocks,
    bool usable_linear_estimate,
    float default_decay,
    bool stationary_block) {
  if (enable_smooth_freq_resp_tail_updates_) {
    if (!stationary_block) {
      float alpha = 0;
      if (quality_linear) {
        alpha = 0.2f * quality_linear.value();
        UpdateFreqRespTail(filter_freq_response, filter_delay_blocks, alpha);
      }
      if (IsAGoodFilterForDecayEstimation(filter_delay_blocks,
                                          usable_linear_estimate,
                                          impulse_response.size())) {
        alpha_ = std::max(alpha, alpha_);
        if ((alpha_ > 0.f) && (default_decay < 0.f)) {
          // Echo tail decay estimation if default_decay is negative.
          UpdateReverbDecay(impulse_response);
        }
      } else {
        ResetDecayEstimation();
      }
    }
  } else {
    UpdateFreqRespTail(filter_freq_response, filter_delay_blocks, 0.1f);
  }
}

void ReverbModelEstimator::ResetDecayEstimation() {
  linear_regressor_.Reset();
  current_reverb_decay_section_ = 0;
  num_reverb_decay_sections_ = 0;
  num_reverb_decay_sections_next_ = 0;
  found_end_of_reverb_decay_ = false;
  alpha_ = 0.f;
}

void ReverbModelEstimator::UpdateReverbDecay(
    const std::vector<float>& impulse_response) {
  constexpr float kOneByFftLengthBy2 = 1.f / kFftLengthBy2;

  if (current_reverb_decay_section_ < filter_main_length_blocks_) {
    std::array<float, kFftLengthBy2> matching_data;
    const size_t start_index = current_reverb_decay_section_ * kFftLengthBy2;
    std::transform(impulse_response.begin() + start_index,
                   impulse_response.begin() + start_index + kFftLengthBy2,
                   matching_data.begin(),
                   [](float a) {
                     return a * a;
                   });  // TODO(devicentepena) check if focusing
                        // on one block would be enough.

    // Update accumulated variables for the current filter section.
    float section_energy =
        std::accumulate(matching_data.begin(), matching_data.end(), 0.f) *
        kOneByFftLengthBy2;

    section_energy = std::max(
        section_energy, 1e-32f);  // Regularization to avoid division by 0.

    RTC_DCHECK_LT(current_reverb_decay_section_, block_energies_.size());
    const float energy_ratio =
        block_energies_[current_reverb_decay_section_] / section_energy;

    found_end_of_reverb_decay_ = found_end_of_reverb_decay_ ||
                                 (energy_ratio > 1.1f || energy_ratio < 0.9f);

    // Count consecutive number of "good" filter sections, where "good" means:
    // 1) energy is above noise floor.
    // 2) energy of current section has not changed too much from last check.
    if (!found_end_of_reverb_decay_ && section_energy > tail_energy_) {
      ++num_reverb_decay_sections_next_;
    } else {
      found_end_of_reverb_decay_ = true;
    }

    block_energies_[current_reverb_decay_section_] = section_energy;

    if (num_reverb_decay_sections_ > 0) {
      linear_regressor_sections_.StartBlock(current_reverb_decay_section_ -
                                            peak_index_ -
                                            kBlocksFirstReflections);
      // Linear regression of log squared magnitude of impulse response.
      for (size_t i = 0; i < kFftLengthBy2; i++) {
        float z = FastApproxLog2f(matching_data[i] + 1e-10);
        if (current_reverb_decay_section_ >= block_after_early_reflections_) {
          linear_regressor_.Update(z);
        }
        linear_regressor_sections_.Update(z);
      }
      linear_regressor_sections_.EndBlock(
          current_reverb_decay_section_ - peak_index_ - kBlocksFirstReflections,
          alpha_);
    }

    num_reverb_decay_sections_ =
        num_reverb_decay_sections_ > 0 ? num_reverb_decay_sections_ - 1 : 0;
    ++current_reverb_decay_section_;

  } else {
    // Form the data to match against by squaring the impulse response
    // coefficients.
    std::array<float, GetTimeDomainLength(kMaxAdaptiveFilterLength)>
        matching_data_data;
    RTC_DCHECK_LE(GetTimeDomainLength(filter_main_length_blocks_),
                  matching_data_data.size());
    rtc::ArrayView<float> matching_data(
        matching_data_data.data(),
        GetTimeDomainLength(filter_main_length_blocks_));
    std::transform(
        impulse_response.begin(), impulse_response.end(), matching_data.begin(),
        [](float a) { return a * a; });  // TODO(devicentepena) This could be
                                         // optimized further and not square the
                                         // whole impulse reponse.

    // Accumulated variables throughout whole filter.
    // Solve for decay rate.
    float decay = linear_regressor_.EstimateDecay(reverb_decay_);

    // Filter tail energy (assumed to be noise).
    constexpr size_t kTailLength = kFftLengthBy2;

    constexpr float k1ByTailLength = 1.f / kTailLength;
    const size_t tail_index =
        GetTimeDomainLength(filter_main_length_blocks_) - kTailLength;

    RTC_DCHECK_GT(matching_data.size(), tail_index);

    tail_energy_ = std::accumulate(matching_data.begin() + tail_index,
                                   matching_data.end(), 0.f) *
                   k1ByTailLength;

    // Update length of decay.
    num_reverb_decay_sections_ = num_reverb_decay_sections_next_;
    num_reverb_decay_sections_next_ = 0;
    // Must have enough data (number of sections) in order
    // to estimate decay rate.
    if (num_reverb_decay_sections_ < 5) {
      num_reverb_decay_sections_ = 0;
    }

    // Identify the peak index of the impulse response.
    const size_t peak_index = std::distance(
        matching_data.begin(),
        std::max_element(matching_data.begin(), matching_data.end()));
    peak_index_ = peak_index * kOneByFftLengthBy2;
    current_reverb_decay_section_ = peak_index_ + kBlocksFirstReflections;
    size_t num_reverb_decay_sections = num_reverb_decay_sections_;
    size_t early_reflections = linear_regressor_sections_.EarlyReflections();
    if (num_reverb_decay_sections - early_reflections > 5) {
      block_after_early_reflections_ =
          current_reverb_decay_section_ + early_reflections;
      num_reverb_decay_sections -= early_reflections;
    } else {
      // Not enough blocks for the decay estimate, do not use that
      // current realization of the filter.
      num_reverb_decay_sections = 0;
      num_reverb_decay_sections_ = 0;
    }

    const float N = num_reverb_decay_sections * kFftLengthBy2;
    linear_regressor_.InitAccumulators(N);

    // Make sure we're not out of bounds.
    if (current_reverb_decay_section_ + 1 >= filter_main_length_blocks_) {
      current_reverb_decay_section_ = filter_main_length_blocks_;
    }
    size_t start_index = current_reverb_decay_section_ * kFftLengthBy2;
    float first_section_energy =
        std::accumulate(matching_data.begin() + start_index,
                        matching_data.begin() + start_index + kFftLengthBy2,
                        0.f) *
        kOneByFftLengthBy2;

    // To estimate the reverb decay, the energy of the first filter section
    // must be substantially larger than the last.
    // Also, the first filter section energy must not deviate too much
    // from the max peak.
    bool main_filter_has_reverb = first_section_energy > 4.f * tail_energy_;
    bool main_filter_is_sane = first_section_energy > 2.f * tail_energy_ &&
                               matching_data[peak_index] < 100.f;

    if (main_filter_is_sane && num_reverb_decay_sections_ > 0) {
      constexpr float kMaxDecay = 0.95f;  // ~1 sec min RT60.
      constexpr float kMinDecay = 0.02f;  // ~15 ms max RT60.
      decay = std::max(.97f * reverb_decay_, decay);
      decay = std::min(decay, kMaxDecay);
      decay = std::max(decay, kMinDecay);
      reverb_decay_ -= alpha_ * (reverb_decay_ - decay);
    }

    found_end_of_reverb_decay_ =
        !(main_filter_is_sane && main_filter_has_reverb);
    alpha_ = 0.f;  // Stop estimation of the decay until another good filter is
                   // received
  }
}

// Updates the estimation of the frequency response at the filter tail.
void ReverbModelEstimator::UpdateFreqRespTail(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_freq_response,
    int filter_delay_blocks,
    float alpha) {
  size_t num_blocks = filter_freq_response.size();
  rtc::ArrayView<const float> freq_resp_tail(
      filter_freq_response[num_blocks - 1]);
  rtc::ArrayView<const float> freq_resp_direct_path(
      filter_freq_response[filter_delay_blocks]);
  float ratio_energies =
      ComputeRatioEnergies(freq_resp_direct_path, freq_resp_tail);
  ratio_tail_to_direct_path_ +=
      alpha * (ratio_energies - ratio_tail_to_direct_path_);

  for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
    freq_resp_tail_[k] = freq_resp_direct_path[k] * ratio_tail_to_direct_path_;
  }

  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    float avg_neighbour =
        0.5f * (freq_resp_tail_[k - 1] + freq_resp_tail_[k + 1]);
    freq_resp_tail_[k] = std::max(freq_resp_tail_[k], avg_neighbour);
  }
}

void ReverbModelEstimator::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) {
  data_dumper->DumpRaw("aec3_reverb_decay", reverb_decay_);
  data_dumper->DumpRaw("aec3_reverb_tail_energy", tail_energy_);
  data_dumper->DumpRaw("aec3_reverb_alpha", alpha_);
  data_dumper->DumpRaw("aec3_num_reverb_decay_sections",
                       static_cast<int>(num_reverb_decay_sections_));
  data_dumper->DumpRaw("aec3_blocks_after_early_reflections",
                       (int)block_after_early_reflections_);
  linear_regressor_sections_.Dump(data_dumper);
}

void ReverbModelEstimator::LinearRegressor::Reset() {
  accumulated_nz_ = 0.f;
  accumulated_nn_ = 0.f;
  accumulated_count_ = 0.f;
}

void ReverbModelEstimator::LinearRegressor::InitAccumulators(const float N) {
  accumulated_nz_ = 0.f;

  // Arithmetic sum $2 \sum_{i=0.5}^{(N-1)/2}i^2$ calculated directly.
  accumulated_nn_ = N * (N * N - 1.0f) * (1.f / 12.f);
  accumulated_count_ = -N * 0.5f;
  // Linear regression approach assumes symmetric index around 0.
  accumulated_count_ += 0.5f;
}

void ReverbModelEstimator::LinearRegressor::Update(float z) {
  accumulated_nz_ += accumulated_count_ * z;
  ++accumulated_count_;
}

float ReverbModelEstimator::LinearRegressor::EstimateDecay(
    float decay_fallback) {
  float decay = decay_fallback;
  if (accumulated_nn_ != 0) {
    const float exp_candidate = -accumulated_nz_ / accumulated_nn_;
    decay = (powf(2.0f, -exp_candidate * kFftLengthBy2));
  }
  return decay;
}

ReverbModelEstimator::LinearRegressorSections::LinearRegressorSections(
    size_t blocks)
    : linear_regressors_(1 + (blocks / kBlocksPerSection)),
      numerators_(1 + (blocks / kBlocksPerSection), 0.f) {
  for (auto lr : linear_regressors_) {
    lr.Reset();
  }
}
ReverbModelEstimator::LinearRegressorSections::~LinearRegressorSections() =
    default;

void ReverbModelEstimator::LinearRegressorSections::StartBlock(
    size_t current_block) {
  idx_ = current_block / kBlocksPerSection;
  bool start_section = (idx_ * kBlocksPerSection == current_block);
  RTC_DCHECK_GE(idx_, 0);
  RTC_DCHECK_LT(idx_, linear_regressors_.size());
  if (start_section) {
    linear_regressors_[idx_].InitAccumulators(kBlocksPerSection *
                                              kFftLengthBy2);
    section_started_ = true;
  }
}

void ReverbModelEstimator::LinearRegressorSections::EndBlock(
    size_t current_block,
    float alpha) {
  idx_ = current_block / kBlocksPerSection;
  bool end_section = ((idx_ + 1) * kBlocksPerSection - 1 == current_block);
  if (end_section) {
    if (section_started_ == false) {
      linear_regressors_[idx_]
          .Reset();  // Do not use the estimation for this section as
                     // the starting block was not included
    }
    section_started_ = false;
    // Updating last section
    numerators_[idx_] +=
        alpha * (linear_regressors_[idx_].GetAccumulatedNumerator() -
                 numerators_[idx_]);
  }
}

void ReverbModelEstimator::LinearRegressorSections::Update(float z) {
  linear_regressors_[idx_].Update(z);
}

void ReverbModelEstimator::LinearRegressorSections::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) {
  data_dumper->DumpRaw(
      "aec3_lr_acum_numerator",
      rtc::ArrayView<const float>(numerators_.data(), numerators_.size()));
}

size_t ReverbModelEstimator::LinearRegressorSections::EarlyReflections() {
  size_t early_reflections = 0;

  constexpr float N = kBlocksPerSection * kFftLengthBy2;
  constexpr float accumulated_nn = N * (N * N - 1.0f) * (1.f / 12.f);
  // numerator_11 refers to the quantity that the linear regressor needs in the
  // numerator for getting a decay equal to 1.1 (which is not a decay) log2(1.1)
  constexpr float numerator_11 =
      0.13750352374993502f * accumulated_nn / kFftLengthBy2;
  // log2(0.8) *  accumulated_nn / kFftLengthBy2
  constexpr float numerator_08 =
      -0.32192809488736229f * accumulated_nn / kFftLengthBy2;
  constexpr size_t sections_to_analyze = 3;

  // Analyzing the first sections_to_analyze regions.
  auto min_num_stable_region = std::min_element(
      numerators_.begin() + sections_to_analyze, numerators_.end());
  for (size_t k = 0; k < sections_to_analyze; ++k) {
    if ((numerators_[k] > numerator_11) ||   // Blocks with positive tilts.
        ((numerators_[k] < numerator_08) &&  // Tilts that are significantly
                                             // steeper than the remaining
                                             // impulse response.
         (numerators_[k] < 0.9f * (*min_num_stable_region)))) {
      early_reflections = (k + 1) * kBlocksPerSection;
    }
  }

  return early_reflections;
}
}  // namespace webrtc
