/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/subband_erle_estimator.h"

#include <algorithm>
#include <memory>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
constexpr int kErleHold = 100;
constexpr int kBlocksForOnsetDetection = kErleHold + 150;

bool EnableAdaptErleOnLowRender() {
  return !field_trial::IsEnabled("WebRTC-Aec3AdaptErleOnLowRenderKillSwitch");
}

}  // namespace

SubbandErleEstimator::SubbandErleEstimator(float min_erle,
                                           float max_erle_lf,
                                           float max_erle_hf,
                                           size_t num_estimators,
                                           size_t main_filter_length_blocks,
                                           size_t delay_headroom_blocks)
    : min_erle_(min_erle),
      max_erle_lf_(max_erle_lf),
      max_erle_hf_(max_erle_hf),
      correction_factor_estimator_(num_estimators,
                                   main_filter_length_blocks,
                                   delay_headroom_blocks),
      adapt_on_low_render_(EnableAdaptErleOnLowRender()) {
  Reset();
}

SubbandErleEstimator::~SubbandErleEstimator() = default;

void SubbandErleEstimator::Reset() {
  erle_.fill(min_erle_);
  erle_for_echo_estimate_.fill(min_erle_);
  erle_onsets_.fill(min_erle_);
  coming_onset_.fill(true);
  hold_counters_.fill(0);
  accum_spectra_.Reset();
  correction_factor_estimator_.Reset(min_erle_);
}

void SubbandErleEstimator::Update(
    const RenderBuffer& render_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    rtc::ArrayView<const float> X2,
    rtc::ArrayView<const float> Y2,
    rtc::ArrayView<const float> E2,
    bool converged_filter,
    bool onset_detection) {
  std::array<size_t, kFftLengthBy2Plus1> n_active_groups;
  correction_factor_estimator_.GetNumberOfActiveFilterGroups(
      render_buffer, filter_frequency_response, n_active_groups);

  constexpr size_t kFftLengthBy4 = kFftLengthBy2 / 2;
  if (converged_filter) {
    // Note that the use of the converged_filter flag already imposed
    // a minimum of the erle that can be estimated as that flag would
    // be false if the filter is performing poorly.
    accum_spectra_.Update(X2, Y2, E2, adapt_on_low_render_);

    UpdateBands(accum_spectra_, 1, kFftLengthBy4, max_erle_lf_,
                onset_detection);
    UpdateBands(accum_spectra_, kFftLengthBy4, kFftLengthBy2, max_erle_hf_,
                onset_detection);

    correction_factor_estimator_.Update(X2, Y2, E2, n_active_groups,
                                        kFftLengthBy4, min_erle_, max_erle_lf_,
                                        max_erle_hf_);
  }

  if (onset_detection) {
    DecreaseErlePerBandForLowRenderSignals();
  }

  erle_[0] = erle_[1];
  erle_[kFftLengthBy2] = erle_[kFftLengthBy2 - 1];

  AdjustErlePerNumberActiveFilterGroups(0, kFftLengthBy4, max_erle_lf_,
                                        n_active_groups);
  AdjustErlePerNumberActiveFilterGroups(kFftLengthBy4, kFftLengthBy2Plus1,
                                        max_erle_hf_, n_active_groups);
}

void SubbandErleEstimator::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  data_dumper->DumpRaw("aec3_erle", erle_for_echo_estimate_);
  data_dumper->DumpRaw("aec3_erle_onset", ErleOnsets());
}

void SubbandErleEstimator::UpdateBands(
    const SubbandErleEstimator::AccumulativeSpectra& accum_spectra,
    size_t start,
    size_t stop,
    float max_erle,
    bool onset_detection) {
  for (size_t k = start; k < stop; ++k) {
    if (accum_spectra.EnoughPoints(k) && accum_spectra.E2_[k]) {
      float new_erle = accum_spectra.Y2_[k] / accum_spectra.E2_[k];
      if (onset_detection && !accum_spectra_.low_render_energy_[k]) {
        if (coming_onset_[k]) {
          coming_onset_[k] = false;
          erle_onsets_[k] = ErleBandUpdate(erle_onsets_[k], new_erle,
                                           accum_spectra_.low_render_energy_[k],
                                           0.15f, 0.3f, min_erle_, max_erle);
        }
        hold_counters_[k] = kBlocksForOnsetDetection;
      }
      erle_[k] = ErleBandUpdate(
          erle_[k], new_erle, accum_spectra_.low_render_energy_[k],
          kSmthConstantIncreases, kSmthConstantDecreases, min_erle_, max_erle);
    }
  }
}

void SubbandErleEstimator::DecreaseErlePerBandForLowRenderSignals() {
  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    hold_counters_[k]--;
    if (hold_counters_[k] <= (kBlocksForOnsetDetection - kErleHold)) {
      if (erle_[k] > erle_onsets_[k]) {
        erle_[k] = std::max(erle_onsets_[k], 0.97f * erle_[k]);
        RTC_DCHECK_LE(min_erle_, erle_[k]);
      }
      if (hold_counters_[k] <= 0) {
        coming_onset_[k] = true;
        hold_counters_[k] = 0;
      }
    }
  }
}

void SubbandErleEstimator::AdjustErlePerNumberActiveFilterGroups(
    size_t start,
    size_t stop,
    float max_erle,
    rtc::ArrayView<const size_t> n_active_groups) {
  for (size_t k = start; k < stop; ++k) {
    float correction_factor_group =
        correction_factor_estimator_.GetCorrectionFactor(k, n_active_groups[k]);
    erle_for_echo_estimate_[k] =
        rtc::SafeClamp(erle_[k] * correction_factor_group, min_erle_, max_erle);
  }
}

SubbandErleEstimator::AccumulativeSpectra::AccumulativeSpectra() {
  Reset();
}

void SubbandErleEstimator::AccumulativeSpectra::Reset() {
  Y2_.fill(0.f);
  E2_.fill(0.f);
  num_points_.fill(0);
  low_render_energy_.fill(false);
}

void SubbandErleEstimator::AccumulativeSpectra::Update(
    rtc::ArrayView<const float> X2,
    rtc::ArrayView<const float> Y2,
    rtc::ArrayView<const float> E2,
    bool update_on_low_render) {
  for (size_t k = 0; k < X2.size(); ++k) {
    if (update_on_low_render || X2[k] > kX2BandEnergyThreshold) {
      if (num_points_[k] == kPointsToAccumulate) {
        Y2_[k] = 0.f;
        E2_[k] = 0.f;
        num_points_[k] = 0;
        low_render_energy_[k] = false;
      }
      low_render_energy_[k] =
          low_render_energy_[k] || X2[k] < kX2BandEnergyThreshold;
      Y2_[k] += Y2[k];
      E2_[k] += E2[k];
      num_points_[k]++;
    }
  }
}

void SubbandErleEstimator::AccumulativeSpectra::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  data_dumper->DumpRaw("aec3_E2_acum", E2_);
  data_dumper->DumpRaw("aec3_Y2_acum", Y2_);
}

}  // namespace webrtc
