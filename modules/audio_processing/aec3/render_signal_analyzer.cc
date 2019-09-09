/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_signal_analyzer.h"

#include <math.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
constexpr size_t kCounterThreshold = 5;

// Identifies local bands with narrow characteristics.
void IdentifySmallNarrowBandRegions(
    const RenderBuffer& render_buffer,
    const absl::optional<size_t>& delay_partitions,
    std::vector<std::array<size_t, kFftLengthBy2 - 1>>* narrow_band_counters) {
  RTC_DCHECK(narrow_band_counters);

  if (!delay_partitions) {
    for (auto& counters : *narrow_band_counters) {
      counters.fill(0);
    }
    return;
  }

  RTC_DCHECK_EQ(narrow_band_counters->size(), render_buffer.Block(0)[0].size());
  for (size_t channel = 0; channel < narrow_band_counters->size(); ++channel) {
    rtc::ArrayView<const float> X2 =
        render_buffer.Spectrum(*delay_partitions, channel);
    RTC_DCHECK_EQ(kFftLengthBy2Plus1, X2.size());
    rtc::ArrayView<size_t> counters = (*narrow_band_counters)[channel];
    for (size_t k = 1; k < (X2.size() - 1); ++k) {
      counters[k - 1] =
          X2[k] > 3 * std::max(X2[k - 1], X2[k + 1]) ? counters[k - 1] + 1 : 0;
    }
  }
}

// Identifies whether the signal has a single strong narrow-band component.
void IdentifyStrongNarrowBandComponent(const RenderBuffer& render_buffer,
                                       int strong_peak_freeze_duration,
                                       absl::optional<int>* narrow_peak_band,
                                       size_t* narrow_peak_counter) {
  RTC_DCHECK(narrow_peak_band);
  RTC_DCHECK(narrow_peak_counter);
  if (*narrow_peak_band &&
      ++(*narrow_peak_counter) >
          static_cast<size_t>(strong_peak_freeze_duration)) {
    *narrow_peak_band = absl::nullopt;
  }

  const std::vector<std::vector<std::vector<float>>>& x_latest =
      render_buffer.Block(0);
  for (size_t channel = 0; channel < x_latest[0].size(); ++channel) {
    const auto X2_latest = render_buffer.Spectrum(0, channel);

    // Identify the spectral peak.
    const int peak_bin =
        static_cast<int>(std::max_element(X2_latest.begin(), X2_latest.end()) -
                         X2_latest.begin());

    // Compute the level around the peak.
    float non_peak_power = 0.f;
    for (int k = std::max(0, peak_bin - 14); k < peak_bin - 4; ++k) {
      non_peak_power = std::max(X2_latest[k], non_peak_power);
    }
    for (int k = peak_bin + 5;
         k < std::min(peak_bin + 15, static_cast<int>(kFftLengthBy2Plus1));
         ++k) {
      non_peak_power = std::max(X2_latest[k], non_peak_power);
    }

    // Assess the render signal strength.
    auto result0 = std::minmax_element(x_latest[0][channel].begin(),
                                       x_latest[0][channel].end());
    float max_abs = std::max(fabs(*result0.first), fabs(*result0.second));

    if (x_latest.size() > 1) {
      const auto result1 = std::minmax_element(x_latest[1][channel].begin(),
                                               x_latest[1][channel].end());
      max_abs =
          std::max(max_abs, static_cast<float>(std::max(
                                fabs(*result1.first), fabs(*result1.second))));
    }

    // Detect whether the spectral peak has as strong narrowband nature.
    if (peak_bin > 0 && max_abs > 100 &&
        X2_latest[peak_bin] > 100 * non_peak_power) {
      *narrow_peak_band = peak_bin;
      *narrow_peak_counter = 0;
    }
  }
}
}  // namespace

RenderSignalAnalyzer::RenderSignalAnalyzer(const EchoCanceller3Config& config,
                                           size_t num_channels)
    : strong_peak_freeze_duration_(config.filter.main.length_blocks),
      narrow_band_counters_(num_channels) {}

RenderSignalAnalyzer::~RenderSignalAnalyzer() = default;

void RenderSignalAnalyzer::Update(
    const RenderBuffer& render_buffer,
    const absl::optional<size_t>& delay_partitions) {
  // Identify bands of narrow nature.
  IdentifySmallNarrowBandRegions(render_buffer, delay_partitions,
                                 &narrow_band_counters_);

  // Identify the presence of a strong narrow band.
  IdentifyStrongNarrowBandComponent(render_buffer, strong_peak_freeze_duration_,
                                    &narrow_peak_band_, &narrow_peak_counter_);
}

// Returns true if the render signal is poorly exciting.
bool RenderSignalAnalyzer::PoorSignalExcitation() const {
  for (const auto& counters : narrow_band_counters_) {
    RTC_DCHECK_LT(2, counters.size());
    for (size_t c : counters) {
      if (c > 10) {
        return true;
      }
    }
  }
  return false;
}

void RenderSignalAnalyzer::MaskRegionsAroundNarrowBands(
    std::array<float, kFftLengthBy2Plus1>* v) const {
  RTC_DCHECK(v);

  // Set v to zero around narrow band signal regions.
  for (const auto& counters : narrow_band_counters_) {
    if (counters[0] > kCounterThreshold) {
      (*v)[1] = (*v)[0] = 0.f;
    }
    for (size_t k = 2; k < kFftLengthBy2 - 1; ++k) {
      if (counters[k - 1] > kCounterThreshold) {
        (*v)[k - 2] = (*v)[k - 1] = (*v)[k] = (*v)[k + 1] = (*v)[k + 2] = 0.f;
      }
    }
    if (counters[kFftLengthBy2 - 2] > kCounterThreshold) {
      (*v)[kFftLengthBy2] = (*v)[kFftLengthBy2 - 1] = 0.f;
    }
  }
}

}  // namespace webrtc
