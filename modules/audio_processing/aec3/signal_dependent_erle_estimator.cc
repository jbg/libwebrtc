/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/signal_dependent_erle_estimator.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/vector_buffer.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {

constexpr float kX2BandEnergyThreshold = 44015068.0f;
constexpr float kSmthConstantDecreases = 0.1f;
constexpr float kSmthConstantIncreases = kSmthConstantDecreases / 2.f;
constexpr size_t
    bands_boundaries_[SignalDependentErleEstimator::kSubbands + 1] = {
        1, 8, 16, 24, 32, 48, kFftLengthBy2Plus1};

// DefineFilterSectionSizes defines the size in blocks of the sections that are
// used for dividing the linear filter. The sections are splitted in a non
// linear manner so lower sections that typically represent the direct path
// have a larger resolution than the higher regions which typically represent
// more reverberant acoustic paths.
void DefineFilterSectionSizes(size_t num_sections,
                              size_t filter_length_blocks,
                              rtc::ArrayView<size_t> estimator_sizes_blocks) {
  size_t remaining_blocks = filter_length_blocks;
  size_t remaining_estimators = num_sections;
  size_t estimator_size = 2;
  size_t idx = 0;
  while (remaining_estimators > 1 &&
         remaining_blocks / remaining_estimators > estimator_size) {
    RTC_DCHECK_LT(idx, estimator_sizes_blocks.size());
    estimator_sizes_blocks[idx] = estimator_size;
    remaining_blocks -= estimator_size;
    remaining_estimators--;
    estimator_size *= 2;
    idx++;
  }

  size_t last_groups_size = remaining_blocks / remaining_estimators;
  for (; idx < num_sections; idx++) {
    estimator_sizes_blocks[idx] = last_groups_size;
  }
  estimator_sizes_blocks[num_sections - 1] +=
      remaining_blocks - last_groups_size * remaining_estimators;
}

// SetSectonsBoundaries writes the block number limits for each filter section
// in the ArrayView estimator_boundaries_blocks.
void SetSectionsBoundaries(size_t delay_headroom_blocks,
                           size_t num_blocks,
                           rtc::ArrayView<const size_t> estimator_sizes_blocks,
                           rtc::ArrayView<size_t> estimator_boundaries_blocks) {
  if (estimator_boundaries_blocks.size() == 2) {
    estimator_boundaries_blocks[0] = 0;
    estimator_boundaries_blocks[1] = num_blocks;
    return;
  }
  size_t idx = 0;
  size_t current_size_block = 0;
  RTC_DCHECK_EQ(estimator_sizes_blocks.size() + 1,
                estimator_boundaries_blocks.size());
  estimator_boundaries_blocks[0] = delay_headroom_blocks;
  for (size_t k = delay_headroom_blocks; k < num_blocks; ++k) {
    current_size_block++;
    if (current_size_block >= estimator_sizes_blocks[idx]) {
      idx = idx + 1;
      if (idx == estimator_sizes_blocks.size()) {
        break;
      }
      estimator_boundaries_blocks[idx] = k + 1;
      current_size_block = 0;
    }
  }
  estimator_boundaries_blocks[estimator_sizes_blocks.size()] = num_blocks;
}

}  // namespace

SignalDependentErleEstimator::SignalDependentErleEstimator(
    const EchoCanceller3Config& config)
    : min_erle_(config.erle.min),
      max_erle_lf_(config.erle.max_l),
      max_erle_hf_(config.erle.max_h),
      num_sections_(config.erle.num_sections),
      num_blocks_(config.filter.main.length_blocks),
      delay_headroom_blocks_(config.delay.delay_headroom_blocks),
      S2_section_accum_(num_sections_),
      erle_estimators_(num_sections_),
      correction_factors_(num_sections_),
      section_boundaries_blocks_(num_sections_ + 1) {
  RTC_DCHECK_LE(num_sections_, num_blocks_);
  RTC_DCHECK_GE(num_sections_, 1);
  std::vector<size_t> section_sizes(num_sections_);
  // Sets the sections used for dividing the linear filter. Those sections
  // are used for analyzing the echo estimates and investigating which
  // linear filter sections contributes most to the echo estimate energy.
  DefineFilterSectionSizes(num_sections_, num_blocks_ - delay_headroom_blocks_,
                           section_sizes);
  SetSectionsBoundaries(delay_headroom_blocks_, num_blocks_, section_sizes,
                        section_boundaries_blocks_);

  Reset();
}

SignalDependentErleEstimator::~SignalDependentErleEstimator() = default;

void SignalDependentErleEstimator::Reset() {
  erle_.fill(min_erle_);
  for (auto& erle : erle_estimators_) {
    erle.fill(min_erle_);
  }
  erle_ref_.fill(min_erle_);
  for (auto& factor : correction_factors_) {
    factor.fill(1.0f);
  }
  num_updates_.fill(0);
}

void SignalDependentErleEstimator::Update(
    const RenderBuffer& render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    rtc::ArrayView<const float> X2,
    rtc::ArrayView<const float> Y2,
    rtc::ArrayView<const float> E2,
    rtc::ArrayView<const float> erle_in,
    bool converged_filter) {
  if (num_sections_ <= 1) {
    return;
  }

  // Gets the number of filter sections that are needed for achieving the 90 %
  // of the power spectrum energy of the echo estimate.
  std::array<size_t, kFftLengthBy2Plus1> n_active_sections;
  GetNumberOfActiveFilterSections(render_buffer, filter_frequency_response,
                                  n_active_sections);

  if (converged_filter) {
    // Updates the correction factor that is used for correcting the erle and
    // adapt it to the particular characteristics of the input signal.
    UpdateCorrectionFactors(X2, Y2, E2, n_active_sections);
  }

  // Applies the correction factor to the input erle for getting a more refine
  // erle estimation for the current input signal.
  for (size_t k = 0; k < kFftLengthBy2; ++k) {
    float max_erle = k < kFftLengthBy2 / 2 ? max_erle_lf_ : max_erle_hf_;
    float correction_factor = GetCorrectionFactor(k, n_active_sections[k]);
    erle_[k] =
        rtc::SafeClamp(erle_in[k] * correction_factor, min_erle_, max_erle);
  }
}

void SignalDependentErleEstimator::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  for (auto& erle : erle_estimators_) {
    data_dumper->DumpRaw("aec3_all_erle", erle);
  }
  data_dumper->DumpRaw("aec3_ref_erle", erle_ref_);
  for (auto& factor : correction_factors_) {
    data_dumper->DumpRaw("aec3_erle_correction_factor", factor);
  }
  data_dumper->DumpRaw("aec3_erle", erle_);
}

// The method GetNumberOfActiveFilterSections estimates which region of the
// linear filter is responsible for getting the majority of the echo estimate
// energy. That estimation is returned as the number of filter sections that are
// used for reaching such energy target.
void SignalDependentErleEstimator::GetNumberOfActiveFilterSections(
    const RenderBuffer& render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    rtc::ArrayView<size_t> n_active_filter_sections) {
  if (num_sections_ == 1) {
    std::fill(n_active_filter_sections.begin(), n_active_filter_sections.end(),
              0);
    return;
  }
  // Computes an approximation of the power spectrum if the filter would have
  // been limited to a certain number of filter sections.
  ComputeEchoEstimatePerFilterSection(render_buffer, filter_frequency_response);
  // For each band, computes the number of filter sections that are needed for
  // achieving the 90 % of the echo estimate energy.
  ComputeNumberOfActiveFilterSections(n_active_filter_sections);
}

float SignalDependentErleEstimator::GetCorrectionFactor(
    size_t band,
    size_t n_active_sections) {
  RTC_DCHECK_LT(n_active_sections, erle_estimators_.size());
  size_t subband = BandToSubband(band);
  return correction_factors_[n_active_sections][subband];
}

size_t SignalDependentErleEstimator::BandToSubband(size_t band) const {
  for (size_t idx = 1; idx < kSubbands; ++idx) {
    if (band < bands_boundaries_[idx]) {
      return idx - 1;
    }
  }
  return kSubbands - 1;
}

void SignalDependentErleEstimator::UpdateCorrectionFactors(
    rtc::ArrayView<const float> X2,
    rtc::ArrayView<const float> Y2,
    rtc::ArrayView<const float> E2,
    rtc::ArrayView<const size_t> n_active_sections) {
  const size_t subband_lf = BandToSubband(kFftLengthBy2 / 2);
  for (size_t subband = 0; subband < kSubbands; ++subband) {
    RTC_DCHECK_LE(bands_boundaries_[subband + 1], X2.size());
    float X2_subband =
        std::accumulate(X2.begin() + bands_boundaries_[subband],
                        X2.begin() + bands_boundaries_[subband + 1], 0.f);
    float E2_subband =
        std::accumulate(E2.begin() + bands_boundaries_[subband],
                        E2.begin() + bands_boundaries_[subband + 1], 0.f);

    if (X2_subband > kX2BandEnergyThreshold && E2_subband > 0) {
      float Y2_subband =
          std::accumulate(Y2.begin() + bands_boundaries_[subband],
                          Y2.begin() + bands_boundaries_[subband + 1], 0.f);
      float new_erle = Y2_subband / E2_subband;

      // When aggregating the number of active sections in the filter for
      // different bands we choose to take the minimum of all of them. As an
      // example, if for one of the bands it is the direct path its main
      // contributor to the final echo estimate, we consider the direct path is
      // as well the main contributor for the subband that contains that
      // particular band. That aggregate number of sections is used as the
      // identifier of the erle estimator that needs to be updated.
      size_t idx = std::accumulate(
          n_active_sections.begin() + bands_boundaries_[subband],
          n_active_sections.begin() + bands_boundaries_[subband + 1],
          num_blocks_, [](float a, float b) { return std::min(a, b); });

      RTC_DCHECK_LT(idx, erle_estimators_.size());
      float max_erle = subband < subband_lf ? max_erle_lf_ : max_erle_hf_;
      float alpha = new_erle > erle_estimators_[idx][subband]
                        ? kSmthConstantIncreases
                        : kSmthConstantDecreases;
      erle_estimators_[idx][subband] +=
          alpha * (new_erle - erle_estimators_[idx][subband]);
      erle_estimators_[idx][subband] =
          rtc::SafeClamp(erle_estimators_[idx][subband], min_erle_, max_erle);

      alpha = new_erle > erle_ref_[subband] ? kSmthConstantIncreases
                                            : kSmthConstantDecreases;
      erle_ref_[subband] += alpha * (new_erle - erle_ref_[subband]);
      erle_ref_[subband] =
          rtc::SafeClamp(erle_ref_[subband], min_erle_, max_erle);

      constexpr int kNumUpdateThr = 50;
      if (num_updates_[subband] == kNumUpdateThr) {
        // Computes the ratio between the erle that is updated using all the
        // points Vs the erle that is updated only on signals that share the
        // same number of active filter sections.
        float new_correction_factor =
            erle_estimators_[idx][subband] / erle_ref_[subband];

        correction_factors_[idx][subband] +=
            0.1f * (new_correction_factor - correction_factors_[idx][subband]);
      } else {
        ++num_updates_[subband];
      }
    }
  }
}

void SignalDependentErleEstimator::ComputeEchoEstimatePerFilterSection(
    const RenderBuffer& render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response) {
  const VectorBuffer& spectrum_render_buffer =
      render_buffer.GetSpectrumBuffer();

  RTC_DCHECK_EQ(S2_section_accum_.size() + 1,
                section_boundaries_blocks_.size());
  size_t idx_render = render_buffer.Position();
  idx_render = spectrum_render_buffer.OffsetIndex(
      idx_render, section_boundaries_blocks_[0]);

  for (size_t section = 0; section < num_sections_; ++section) {
    std::array<float, kFftLengthBy2Plus1> X2_section;
    std::array<float, kFftLengthBy2Plus1> H2_section;
    X2_section.fill(0.f);
    H2_section.fill(0.f);
    for (size_t block = section_boundaries_blocks_[section];
         block < section_boundaries_blocks_[section + 1]; ++block) {
      const auto& H2 = filter_frequency_response[block];
      const auto& X2 = spectrum_render_buffer.buffer[idx_render];
      for (size_t k = 0; k < H2.size(); ++k) {
        X2_section[k] += X2[k];
        H2_section[k] += H2[k];
      }
      idx_render = spectrum_render_buffer.IncIndex(idx_render);
    }

    for (size_t k = 0; k < H2_section.size(); ++k) {
      S2_section_accum_[section][k] = X2_section[k] * H2_section[k];
    }
  }

  for (size_t section = 1; section < num_sections_; ++section) {
    for (size_t k = 0; k < S2_section_accum_[section].size(); ++k) {
      S2_section_accum_[section][k] += S2_section_accum_[section - 1][k];
    }
  }
}

void SignalDependentErleEstimator::ComputeNumberOfActiveFilterSections(
    rtc::ArrayView<size_t> number_active_filter_sections) {
  constexpr float target = 0.9f;
  std::array<float, kFftLengthBy2Plus1> energy_targets;
  std::transform(S2_section_accum_[num_sections_ - 1].begin(),
                 S2_section_accum_[num_sections_ - 1].end(),
                 energy_targets.begin(),
                 [=](float S2_band) { return S2_band * target; });
  std::array<bool, kFftLengthBy2Plus1> target_found;
  target_found.fill(false);
  for (size_t section = 0; section < num_sections_; ++section) {
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      if (!target_found[k] &&
          S2_section_accum_[section][k] >= energy_targets[k]) {
        number_active_filter_sections[k] = section;
        target_found[k] = true;
      }
    }
  }
}
}  // namespace webrtc
