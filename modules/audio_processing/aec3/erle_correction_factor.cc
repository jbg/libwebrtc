/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erle_correction_factor.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/vector_buffer.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {
constexpr int kNumUpdateThr = 50;

void GetEstimatorBlockSizes(size_t num_estimators,
                            size_t filter_length_blocks,
                            rtc::ArrayView<size_t> estimator_sizes) {
  size_t remaining_blocks = filter_length_blocks;
  size_t remaining_estimators = num_estimators;
  size_t estimator_size = 2;
  size_t idx = 0;
  while ((remaining_estimators > 1) &&
         (remaining_blocks / remaining_estimators > estimator_size)) {
    estimator_sizes[idx] = estimator_size;
    remaining_blocks -= estimator_size;
    remaining_estimators--;
    estimator_size *= 2;
    idx++;
  }

  size_t last_groups_size = remaining_blocks / remaining_estimators;
  for (; idx < num_estimators; idx++) {
    estimator_sizes[idx] = last_groups_size;
  }
  estimator_sizes[num_estimators - 1] +=
      remaining_blocks - last_groups_size * remaining_estimators;
}

void SetEstimatorBoundaries(
    size_t delay_headroom_blocks,
    size_t num_blocks,
    rtc::ArrayView<const size_t> estimator_sizes,
    rtc::ArrayView<size_t> estimator_boundaries_blocks) {
  if (estimator_boundaries_blocks.size() == 1) {
    estimator_boundaries_blocks[0] = 0;
    return;
  }
  size_t idx = 0;
  size_t estimator_size = 0;
  RTC_DCHECK_EQ(estimator_sizes.size(), estimator_boundaries_blocks.size());
  estimator_boundaries_blocks[0] = delay_headroom_blocks;
  for (size_t k = 0; k < num_blocks; ++k) {
    if (k >= delay_headroom_blocks) {
      estimator_size++;
      if (estimator_size >= estimator_sizes[idx]) {
        idx = idx + 1;
        if (idx == estimator_sizes.size()) {
          break;
        }
        estimator_boundaries_blocks[idx] = k + 1;
        estimator_size = 0;
      }
    }
  }
}

void BlockEstimatorDownsample(
    const VectorBuffer& spectrum_render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    size_t first_block,
    size_t last_block,
    rtc::ArrayView<float> X2_downsampled,
    rtc::ArrayView<float> H2_downsampled,
    size_t* idx_render) {
  std::fill(X2_downsampled.begin(), X2_downsampled.end(), 0.f);
  std::fill(H2_downsampled.begin(), H2_downsampled.end(), 0.f);
  for (size_t block = first_block; block < last_block; ++block) {
    const auto& H2 = filter_frequency_response[block];
    const auto& X2 = spectrum_render_buffer.buffer[*idx_render];
    for (size_t k = 0; k < H2.size(); ++k) {
      X2_downsampled[k] += X2[k];
      H2_downsampled[k] += H2[k];
    }
    *idx_render = spectrum_render_buffer.IncIndex(*idx_render);
  }
}

}  // namespace

namespace aec3 {

float ErleBandUpdate(float erle_band,
                     float new_erle,
                     bool low_render_energy,
                     float alpha_inc,
                     float alpha_dec,
                     float min_erle,
                     float max_erle) {
  if (new_erle < erle_band && low_render_energy) {
    // Decreases are not allowed if low render energy signals were used for
    // the erle computation.
    return erle_band;
  }
  float alpha = new_erle > erle_band ? alpha_inc : alpha_dec;
  float erle_band_out = erle_band;
  erle_band_out = erle_band + alpha * (new_erle - erle_band);
  erle_band_out = rtc::SafeClamp(erle_band_out, min_erle, max_erle);
  return erle_band_out;
}

}  // namespace aec3

const size_t
    ErleCorrectionFactor::bands_boundaries_[ErleCorrectionFactor::kSubbands] = {
        1, 8, 16, 24, 32, 48};

ErleCorrectionFactor::ErleCorrectionFactor(size_t num_estimators,
                                           size_t num_blocks,
                                           size_t delay_headroom_blocks)
    : num_estimators_(num_estimators),
      num_blocks_(num_blocks),
      delay_headroom_blocks_(delay_headroom_blocks),
      S2_block_acum_(num_estimators_),
      erle_estimators_(num_estimators_),
      correction_factor_(num_estimators_),
      estimator_boundaries_blocks_(num_estimators_) {
  DefineEstimatorBoundaries(num_estimators, num_blocks);
}

ErleCorrectionFactor::~ErleCorrectionFactor() = default;

void ErleCorrectionFactor::Reset(float min_erle) {
  for (auto& erle : erle_estimators_) {
    erle.fill(min_erle);
  }
  erle_ref_.fill(min_erle);
  for (auto& factor : correction_factor_) {
    factor.fill(1.0f);
  }
  num_updates_.fill(0);
}

void ErleCorrectionFactor::Update(rtc::ArrayView<const float> X2,
                                  rtc::ArrayView<const float> Y2,
                                  rtc::ArrayView<const float> E2,
                                  rtc::ArrayView<const size_t> n_groups,
                                  int last_band_lf,
                                  float min_erle,
                                  float max_erle_lf,
                                  float max_erle_hf) {
  if (num_estimators_ <= 1) {
    return;
  }
  std::array<float, kSubbands> X2_subbands, Y2_subbands, E2_subbands;
  std::array<size_t, kSubbands> n_groups_subbands;
  GetSubbandDomainVectors(X2, Y2, E2, n_groups, X2_subbands, Y2_subbands,
                          E2_subbands, n_groups_subbands);
  const size_t subband_lf = BandToSubband(last_band_lf);
  UpdateBands(X2_subbands, Y2_subbands, E2_subbands, n_groups_subbands, 0,
              subband_lf, min_erle, max_erle_lf);
  UpdateBands(X2_subbands, Y2_subbands, E2_subbands, n_groups_subbands,
              subband_lf, kSubbands, min_erle, max_erle_hf);
}

void ErleCorrectionFactor::GetNumberOfActiveFilterGroups(
    const RenderBuffer& render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    rtc::ArrayView<size_t> n_active_filter_groups) {
  if (num_estimators_ == 1) {
    std::fill(n_active_filter_groups.begin(), n_active_filter_groups.end(), 0);
    return;
  }
  ComputeBlockContributions(render_buffer, filter_frequency_response);
  ComputeNumberOfActiveFilterGroups(n_active_filter_groups);
}

void ErleCorrectionFactor::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  for (auto& erle : erle_estimators_) {
    data_dumper->DumpRaw("aec3_all_erle", erle);
  }
  data_dumper->DumpRaw("aec3_ref_erle", erle_ref_);
  for (auto& factor : correction_factor_) {
    data_dumper->DumpRaw("aec3_erle_correction_factor", factor);
  }
}

void ErleCorrectionFactor::GetSubbandDomainVectors(
    rtc::ArrayView<const float> X2,
    rtc::ArrayView<const float> Y2,
    rtc::ArrayView<const float> E2,
    rtc::ArrayView<const size_t> n_groups,
    rtc::ArrayView<float> X2_subbands,
    rtc::ArrayView<float> Y2_subbands,
    rtc::ArrayView<float> E2_subbands,
    rtc::ArrayView<size_t> n_groups_subbands) {
  std::fill(X2_subbands.begin(), X2_subbands.end(), 0.f);
  std::fill(Y2_subbands.begin(), Y2_subbands.end(), 0.f);
  std::fill(E2_subbands.begin(), E2_subbands.end(), 0.f);
  std::fill(n_groups_subbands.begin(), n_groups_subbands.end(), num_blocks_);

  for (size_t subband = 0; subband < kSubbands; ++subband) {
    size_t init_band = bands_boundaries_[subband];
    size_t end_band = subband < (kSubbands - 1) ? bands_boundaries_[subband + 1]
                                                : X2.size() - 1;
    for (size_t k = init_band; k < end_band; ++k) {
      X2_subbands[subband] += X2[k];
      Y2_subbands[subband] += Y2[k];
      E2_subbands[subband] += E2[k];
      n_groups_subbands[subband] =
          std::min(n_groups_subbands[subband], n_groups[k]);
    }
  }
}

void ErleCorrectionFactor::UpdateBands(
    rtc::ArrayView<const float> X2_subband,
    rtc::ArrayView<const float> Y2_subband,
    rtc::ArrayView<const float> E2_subband,
    rtc::ArrayView<const size_t> n_groups_subbands,
    size_t subband_start,
    size_t subband_stop,
    float min_erle,
    float max_erle) {
  for (size_t subband = subband_start; subband < subband_stop; ++subband) {
    if (X2_subband[subband] > aec3::kX2BandEnergyThreshold &&
        E2_subband[subband] > 0) {
      float new_erle = Y2_subband[subband] / E2_subband[subband];
      size_t idx = GetEstimatorIndex(n_groups_subbands[subband]);
      rtc::ArrayView<float> erle_to_update = erle_estimators_[idx];
      erle_to_update[subband] = aec3::ErleBandUpdate(
          erle_to_update[subband], new_erle, false,
          aec3::kSmthConstantIncreases, aec3::kSmthConstantDecreases, min_erle,
          max_erle);
      erle_ref_[subband] = aec3::ErleBandUpdate(
          erle_ref_[subband], new_erle, false, aec3::kSmthConstantIncreases,
          aec3::kSmthConstantDecreases, min_erle, max_erle);
      num_updates_[subband] =
          std::min(num_updates_[subband] + 1, kNumUpdateThr);
      if (num_updates_[subband] >= kNumUpdateThr) {
        rtc::ArrayView<float> correction_factor_to_update =
            correction_factor_[idx];
        float new_correction_factor =
            erle_to_update[subband] / erle_ref_[subband];

        correction_factor_to_update[subband] +=
            0.1f *
            (new_correction_factor - correction_factor_to_update[subband]);
      }
    }
  }
}

void ErleCorrectionFactor::DefineEstimatorBoundaries(size_t num_estimators,
                                                     size_t num_blocks) {
  RTC_DCHECK_LE(num_estimators, num_blocks);
  RTC_DCHECK_GE(num_estimators, 1);
  std::vector<size_t> estimator_sizes(num_estimators);
  GetEstimatorBlockSizes(num_estimators, num_blocks - delay_headroom_blocks_,
                         estimator_sizes);
  SetEstimatorBoundaries(delay_headroom_blocks_, num_blocks_, estimator_sizes,
                         estimator_boundaries_blocks_);
}

void ErleCorrectionFactor::ComputeBlockContributions(
    const RenderBuffer& render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response) {
  const VectorBuffer& spectrum_render_buffer =
      render_buffer.GetSpectrumBuffer();

  std::array<float, kFftLengthBy2Plus1> X2_downsampled;
  std::array<float, kFftLengthBy2Plus1> H2_downsampled;
  size_t idx_render = render_buffer.Position();
  idx_render = spectrum_render_buffer.OffsetIndex(
      idx_render, estimator_boundaries_blocks_[0]);

  for (size_t group = 0; group < estimator_boundaries_blocks_.size(); ++group) {
    size_t first_block = estimator_boundaries_blocks_[group];
    size_t last_block = group < estimator_boundaries_blocks_.size() - 1
                            ? estimator_boundaries_blocks_[group + 1]
                            : num_blocks_;

    BlockEstimatorDownsample(spectrum_render_buffer, filter_frequency_response,
                             first_block, last_block, X2_downsampled,
                             H2_downsampled, &idx_render);
    if (group > 0) {
      for (size_t k = 0; k < H2_downsampled.size(); ++k) {
        S2_block_acum_[group][k] = S2_block_acum_[group - 1][k] +
                                   X2_downsampled[k] * H2_downsampled[k];
      }
    } else {
      for (size_t k = 0; k < H2_downsampled.size(); ++k) {
        S2_block_acum_[group][k] = X2_downsampled[k] * H2_downsampled[k];
      }
    }
  }
}

void ErleCorrectionFactor::ComputeNumberOfActiveFilterGroups(
    rtc::ArrayView<size_t> number_active_filter_groups) {
  constexpr float target = 0.9f;
  std::array<float, kFftLengthBy2Plus1> energy_targets;
  std::transform(S2_block_acum_[num_estimators_ - 1].begin(),
                 S2_block_acum_[num_estimators_ - 1].end(),
                 energy_targets.begin(),
                 [=](float S2_band) { return S2_band * target; });
  std::array<bool, kFftLengthBy2Plus1> target_found;
  target_found.fill(false);
  for (size_t group = 0; group < num_estimators_; ++group) {
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      if (!target_found[k]) {
        if (S2_block_acum_[group][k] >= energy_targets[k]) {
          number_active_filter_groups[k] = group;
          target_found[k] = true;
        }
      }
    }
  }
}

}  // namespace webrtc
