/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SIGNAL_DEPENDENT_ERLE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SIGNAL_DEPENDENT_ERLE_ESTIMATOR_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// This class estimates how the Erle varies depending on the portion of the
// linear filter that is used for getting the majority of the echo estimate
// energy. Depending on the region that is currently used a different correction
// factor is used.
class SignalDependentErleEstimator {
 public:
  explicit SignalDependentErleEstimator(const EchoCanceller3Config& config);

  ~SignalDependentErleEstimator();

  void Reset();

  const std::array<float, kFftLengthBy2Plus1>& Erle() const { return erle_; }

  // Updates the Erle estimate by analyzing the current input signals. It takes
  // the render buffer and the filter frequency response in order to do an
  // estimation of the number of sections of the linear filter that are needed
  // for getting the mayority of the echo estimate energy. Based on that number
  // of sections, it updates the erle estimation by introducing a correction
  // factor to the erle that is given as an input to this method. This input
  // erle required to be an estimation of the average Erle achieved by the
  // linear filter.
  void Update(const RenderBuffer& render_buffer,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  filter_frequency_response,
              rtc::ArrayView<const float> X2,
              rtc::ArrayView<const float> Y2,
              rtc::ArrayView<const float> E2,
              rtc::ArrayView<const float> erle_in,
              bool converged_filter);

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) const;

  static constexpr size_t kSubbands = 6;

 private:
  void GetNumberOfActiveFilterSections(
      const RenderBuffer& render_buffer,
      const std::vector<std::array<float, kFftLengthBy2Plus1>>&
          filter_frequency_response,
      rtc::ArrayView<size_t> n_active_filter_groups);

  float GetCorrectionFactor(size_t band, size_t n_active_groups);

  size_t BandToSubband(size_t band) const;

  void UpdateCorrectionFactors(rtc::ArrayView<const float> X2,
                               rtc::ArrayView<const float> Y2,
                               rtc::ArrayView<const float> E2,
                               rtc::ArrayView<const size_t> n_active_sections);

  void ComputeEchoEstimatePerFilterSection(
      const RenderBuffer& render_buffer,
      const std::vector<std::array<float, kFftLengthBy2Plus1>>&
          filter_frequency_response);

  void ComputeNumberOfActiveFilterSections(
      rtc::ArrayView<size_t> number_active_filter_sections);

  void AdjustErlePerNumberActiveFilterGroups(
      size_t start,
      size_t stop,
      float max_erle,
      rtc::ArrayView<size_t> number_active_filter_groups);

  const float min_erle_;
  const float max_erle_lf_;
  const float max_erle_hf_;

  const size_t num_sections_;
  const size_t num_blocks_;
  const size_t delay_headroom_blocks_;
  std::array<float, kFftLengthBy2Plus1> erle_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2_section_accum_;
  std::vector<std::array<float, kSubbands>> erle_estimators_;
  std::array<float, kSubbands> erle_ref_;
  std::vector<std::array<float, kSubbands>> correction_factors_;
  std::vector<size_t> section_boundaries_blocks_;
  std::array<int, kSubbands> num_updates_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SIGNAL_DEPENDENT_ERLE_ESTIMATOR_H_
