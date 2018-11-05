/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ERLE_CORRECTION_FACTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ERLE_CORRECTION_FACTOR_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

namespace aec3 {
constexpr float kX2BandEnergyThreshold = 44015068.0f;
constexpr float kSmthConstantDecreases = 0.1f;
constexpr float kSmthConstantIncreases = kSmthConstantDecreases / 2.f;

float ErleBandUpdate(float erle_band,
                     float new_erle,
                     bool low_render_energy,
                     float alpha_inc,
                     float alpha_dec,
                     float min_erle,
                     float max_erle);

}  // namespace aec3

// Estimates a correction factor for the Erle estimation. This class estimates
// how the Erle varies depending on the portion of the linear filter that is
// used for getting the majority of the echo estimate energy. Depending on the
// region that is currently used a different correction factor is used.
class ErleCorrectionFactor {
 public:
  ErleCorrectionFactor(size_t num_estimators,
                       size_t num_blocks,
                       size_t delay_headroom_blocks);

  ~ErleCorrectionFactor();

  void Reset(float min_erle);

  // Update the correction factor estimate.
  void Update(rtc::ArrayView<const float> X2,
              rtc::ArrayView<const float> Y2,
              rtc::ArrayView<const float> E2,
              rtc::ArrayView<const size_t> n_groups,
              int last_band_lf,
              float min_erle,
              float max_erle_lf,
              float max_erle_hf);

  // Estimates which region of the linear filter is responsible for getting the
  // majority of the echo estimate energy. That estimation is returned as the
  // number of filter groups that are used for reaching such energy target.
  void GetNumberOfActiveFilterGroups(
      const RenderBuffer& render_buffer,
      const std::vector<std::array<float, kFftLengthBy2Plus1>>&
          filter_frequency_response,
      rtc::ArrayView<size_t> n_active_filter_groups);

  // Returns the correction factor to be appied to the Erle estimator.
  float GetCorrectionFactor(size_t band, size_t n_active_groups) {
    size_t idx = GetEstimatorIndex(n_active_groups);
    size_t subband = BandToSubband(band);
    return correction_factor_[idx][subband];
  }

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) const;

 private:
  static constexpr size_t kSubbands = 6;

  size_t BandToSubband(size_t band) {
    for (size_t idx = 1; idx < kSubbands; ++idx) {
      if (band < bands_boundaries_[idx]) {
        return idx - 1;
      }
    }
    return (kSubbands - 1);
  }

  void GetSubbandDomainVectors(rtc::ArrayView<const float> X2,
                               rtc::ArrayView<const float> Y2,
                               rtc::ArrayView<const float> E2,
                               rtc::ArrayView<const size_t> n_groups,
                               rtc::ArrayView<float> X2_subbands,
                               rtc::ArrayView<float> Y2_subbands,
                               rtc::ArrayView<float> E2_subbands,
                               rtc::ArrayView<size_t> n_groups_subbands);

  void UpdateBands(rtc::ArrayView<const float> X2_subband,
                   rtc::ArrayView<const float> Y2_subband,
                   rtc::ArrayView<const float> E2_subband,
                   rtc::ArrayView<const size_t> n_groups_subbands,
                   size_t subband_start,
                   size_t subband_stop,
                   float min_erle,
                   float max_erle);

  void DefineEstimatorBoundaries(size_t num_estimators, size_t num_blocks);

  void ComputeBlockContributions(
      const RenderBuffer& render_buffer,
      const std::vector<std::array<float, kFftLengthBy2Plus1>>&
          filter_frequency_response);

  void ComputeNumberOfActiveFilterGroups(
      rtc::ArrayView<size_t> number_active_filter_groups);

  size_t GetEstimatorIndex(size_t number_active_filter_group) {
    size_t idx = number_active_filter_group;
    RTC_DCHECK(idx >= 0 && idx < erle_estimators_.size());
    return idx;
  }

  static const size_t bands_boundaries_[kSubbands];
  const size_t num_estimators_;
  const size_t num_blocks_;
  const size_t delay_headroom_blocks_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2_block_acum_;
  std::vector<std::array<float, kSubbands>> erle_estimators_;
  std::array<float, kSubbands> erle_ref_;
  std::vector<std::array<float, kSubbands>> correction_factor_;
  std::vector<size_t> estimator_boundaries_blocks_;
  std::array<int, kSubbands> num_updates_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ERLE_CORRECTION_FACTOR_H_
