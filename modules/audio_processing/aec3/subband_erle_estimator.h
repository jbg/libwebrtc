/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUBBAND_ERLE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUBBAND_ERLE_ESTIMATOR_H_

#include <stddef.h>
#include <array>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/erle_correction_factor.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// Estimates the echo return loss enhancement for each frequency subband.
class SubbandErleEstimator {
 public:
  SubbandErleEstimator(float min_erle,
                       float max_erle_lf,
                       float max_erle_hf,
                       size_t num_estimators,
                       size_t main_filter_length_blocks,
                       size_t delay_headroom_blocks);
  ~SubbandErleEstimator();

  // Resets the ERLE estimator.
  void Reset();

  // Updates the ERLE estimate.
  void Update(const webrtc::RenderBuffer& render_buffer,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  filter_frequency_response,
              rtc::ArrayView<const float> X2,
              rtc::ArrayView<const float> Y2,
              rtc::ArrayView<const float> E2,
              bool converged_filter,
              bool onset_detection);

  // Returns the ERLE estimate.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const {
    return erle_for_echo_estimate_;
  }

  // Returns the ERLE estimate at onsets.
  const std::array<float, kFftLengthBy2Plus1>& ErleOnsets() const {
    return erle_onsets_;
  }

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) const;

 private:
  class AccumulativeSpectra {
   public:
    static constexpr int kPointsToAccumulate = 6;
    AccumulativeSpectra();
    void Reset();
    void Update(rtc::ArrayView<const float> X2,
                rtc::ArrayView<const float> Y2,
                rtc::ArrayView<const float> E2,
                bool update_on_low_render);
    bool EnoughPoints(size_t band) const {
      return num_points_[band] == kPointsToAccumulate;
    }
    void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) const;
    std::array<float, kFftLengthBy2Plus1> Y2_;
    std::array<float, kFftLengthBy2Plus1> E2_;
    std::array<bool, kFftLengthBy2Plus1> low_render_energy_;
    std::array<int, kFftLengthBy2Plus1> num_points_;
  };

  void UpdateBands(const AccumulativeSpectra& accum_spectra,
                   size_t start,
                   size_t stop,
                   float max_erle,
                   bool onset_detection);
  void DecreaseErlePerBandForLowRenderSignals();

  void AdjustErlePerNumberActiveFilterGroups(
      size_t start,
      size_t stop,
      float max_erle,
      rtc::ArrayView<const size_t> n_active_groups);

  const float min_erle_;
  const float max_erle_lf_;
  const float max_erle_hf_;

  AccumulativeSpectra accum_spectra_;
  std::array<float, kFftLengthBy2Plus1> erle_;
  std::array<float, kFftLengthBy2Plus1> erle_for_echo_estimate_;
  ErleCorrectionFactor correction_factor_estimator_;
  std::array<float, kFftLengthBy2Plus1> erle_onsets_;
  std::array<bool, kFftLengthBy2Plus1> coming_onset_;
  std::array<int, kFftLengthBy2Plus1> hold_counters_;
  const bool adapt_on_low_render_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUBBAND_ERLE_ESTIMATOR_H_
