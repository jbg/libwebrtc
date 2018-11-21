/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/filter_analyzer.h"
#include <math.h>

#include <algorithm>
#include <array>
#include <numeric>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

constexpr size_t kNumberBlocksToUpdate = 1;

size_t FindPeakIndex(rtc::ArrayView<const float> filter_time_domain,
                     size_t peak_index_in,
                     size_t start_sample,
                     size_t end_sample) {
  size_t peak_index = std::min(peak_index_in, filter_time_domain.size() - 1);
  float max_h2 =
      filter_time_domain[peak_index] * filter_time_domain[peak_index];
  for (size_t k = start_sample; k < end_sample; ++k) {
    float tmp = filter_time_domain[k] * filter_time_domain[k];
    if (tmp > max_h2) {
      peak_index = k;
      max_h2 = tmp;
    }
  }

  return peak_index;
}

bool EnableFilterPreprocessing() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3FilterAnalyzerPreprocessorKillSwitch");
}

bool EnableIncrementalAnalysis() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3FilterAnalyzerIncrementalAnalysisKillSwitch");
}

}  // namespace

int FilterAnalyzer::instance_count_ = 0;

FilterAnalyzer::FilterAnalyzer(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      use_preprocessed_filter_(EnableFilterPreprocessing()),
      bounded_erl_(config.ep_strength.bounded_erl),
      default_gain_(config.ep_strength.lf),
      active_render_threshold_(config.render_levels.active_render_limit *
                               config.render_levels.active_render_limit *
                               kFftLengthBy2),
      use_incremental_analysis_(EnableIncrementalAnalysis()),
      h_highpass_(GetTimeDomainLength(config.filter.main.length_blocks), 0.f),
      filter_length_blocks_(config.filter.main_initial.length_blocks) {
  Reset();
}

FilterAnalyzer::~FilterAnalyzer() = default;

void FilterAnalyzer::Reset() {
  delay_blocks_ = 0;
  consistent_estimate_ = false;
  blocks_since_reset_ = 0;
  consistent_estimate_ = false;
  consistent_estimate_counter_ = 0;
  consistent_delay_reference_ = -10;
  gain_ = default_gain_;
  ResetRegion();
}

void FilterAnalyzer::Update(rtc::ArrayView<const float> filter_time_domain,
                            const RenderBuffer& render_buffer) {
  SetRegionToAnalyze(filter_time_domain);
  AnalyzeRegion(filter_time_domain, render_buffer);
}

void FilterAnalyzer::AnalyzeRegion(
    rtc::ArrayView<const float> filter_time_domain,
    const RenderBuffer& render_buffer) {
  // Preprocess the filter to avoid issues with low-frequency components in the
  // filter.
  PreProcessFilter(filter_time_domain);
  data_dumper_->DumpRaw("aec3_linear_filter_processed_td", h_highpass_);

  const auto& filter_to_analyze =
      use_preprocessed_filter_ ? h_highpass_ : filter_time_domain;
  RTC_DCHECK_EQ(filter_to_analyze.size(), filter_time_domain.size());

  region_.peak_index_ =
      FindPeakIndex(filter_to_analyze, region_.peak_index_,
                    region_.start_sample_, region_.end_sample_);

  delay_blocks_ = region_.peak_index_ >> kBlockSizeLog2;
  UpdateFilterGain(filter_to_analyze, region_.peak_index_);
  filter_length_blocks_ = filter_time_domain.size() * (1.f / kBlockSize);

  const auto& x = render_buffer.Block(-delay_blocks_)[0];
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  const bool active_render_block = x_energy > active_render_threshold_;

  if (active_render_block) {
    ++consistent_estimate_counter_;
  }

  if (region_.filter_end_reached_) {
    float filter_floor = 0;
    float filter_secondary_peak = 0;
    size_t limit1 = region_.peak_index_ < 64 ? 0 : region_.peak_index_ - 64;
    size_t limit2 = region_.peak_index_ > filter_to_analyze.size() - 129
                        ? 0
                        : region_.peak_index_ + 128;

    for (size_t k = 0; k < limit1; ++k) {
      float abs_h = fabsf(filter_to_analyze[k]);
      filter_floor += abs_h;
      filter_secondary_peak = std::max(filter_secondary_peak, abs_h);
    }

    // TODO(devicentepena): limit the number of samples that are analyzed in the
    // next loop.
    for (size_t k = limit2; k < filter_to_analyze.size(); ++k) {
      float abs_h = fabsf(filter_to_analyze[k]);
      filter_floor += abs_h;
      filter_secondary_peak = std::max(filter_secondary_peak, abs_h);
    }

    filter_floor /= (limit1 + filter_to_analyze.size() - limit2);

    float abs_peak = fabsf(filter_to_analyze[region_.peak_index_]);
    bool significant_peak_index = abs_peak > 10.f * filter_floor &&
                                  abs_peak > 2.f * filter_secondary_peak;

    if (consistent_delay_reference_ != delay_blocks_ ||
        !significant_peak_index) {
      consistent_estimate_counter_ = 0;
      consistent_delay_reference_ = delay_blocks_;
    }
    consistent_estimate_ =
        consistent_estimate_counter_ > 1.5f * kNumBlocksPerSecond;
  }
}

void FilterAnalyzer::UpdateFilterGain(
    rtc::ArrayView<const float> filter_time_domain,
    size_t peak_index) {
  bool sufficient_time_to_converge =
      ++blocks_since_reset_ > 5 * kNumBlocksPerSecond;

  if (sufficient_time_to_converge && consistent_estimate_) {
    gain_ = fabsf(filter_time_domain[peak_index]);
  } else {
    if (gain_) {
      gain_ = std::max(gain_, fabsf(filter_time_domain[peak_index]));
    }
  }

  if (bounded_erl_ && gain_) {
    gain_ = std::max(gain_, 0.01f);
  }
}

void FilterAnalyzer::PreProcessFilter(
    rtc::ArrayView<const float> filter_time_domain) {
  RTC_DCHECK_GE(h_highpass_.capacity(), filter_time_domain.size());
  h_highpass_.resize(filter_time_domain.size());
  // Minimum phase high-pass filter with cutoff frequency at about 600 Hz.
  constexpr std::array<float, 3> h = {{0.7929742f, -0.36072128f, -0.47047766f}};

  std::fill(h_highpass_.begin() + region_.start_sample_,
            h_highpass_.begin() + region_.end_sample_, 0.f);
  for (size_t k = std::max(h.size() - 1, region_.start_sample_);
       k < region_.end_sample_; ++k) {
    for (size_t j = 0; j < h.size(); ++j) {
      h_highpass_[k] += filter_time_domain[k - j] * h[j];
    }
  }
}

void FilterAnalyzer::ResetRegion() {
  region_.peak_index_ = 0;
  region_.start_sample_ = 0;
  region_.end_sample_ = 0;
  region_.filter_end_reached_ = false;
}

void FilterAnalyzer::SetRegionToAnalyze(
    rtc::ArrayView<const float> filter_time_domain) {
  auto& r = region_;
  if (use_incremental_analysis_) {
    r.start_sample_ = r.filter_end_reached_ ? 0 : r.end_sample_;
    r.end_sample_ =
        std::min(r.start_sample_ + kNumberBlocksToUpdate * kBlockSize,
                 filter_time_domain.size());
    r.filter_end_reached_ = r.end_sample_ == filter_time_domain.size();

  } else {
    r.start_sample_ = 0;
    r.end_sample_ = filter_time_domain.size();
    r.filter_end_reached_ = true;
  }
}

}  // namespace webrtc
