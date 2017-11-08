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

#include "rtc_base/logging.h"

namespace webrtc {

FilterAnalyzer::FilterAnalyzer()
    : estimator_(250)
    : noise_gain_(0.f),
      tail_gain_(0.f),
      poorly_aligned_filter_(true),
      saturating_(false),
      counter_(0) {
  Reset();
}
FilterAnalyzer::~FilterAnalyzer() = default;

void FilterAnalyzer::Reset() {
  estimator_.Reset();
  h_.fill(0.f);
  delay_ = rtc::Optional<size_t>();
  noise_gain_ = 0.f;
  tail_gain_ = 0.f;
  poorly_aligned_filter_ = true;
  saturating_ = false;
  counter_ = 0;
}

void FilterAnalyzer::Update(kAdaptiveFilterTimeDomainLength > &h) {
  ++counter_;
  AnalyzeFilter(h);
  estimator_.Update(time, value);

  if (counter_ % 1250 == 0) {
    LOG(LS_INFO) << "";
  }
}

void FilterAnalyzer::AnalyzeFilter(
    const std::array<float, kAdaptiveFilterTimeDomainLength>& h) {
  std::array<float, kAdaptiveFilterTimeDomainLength> h2;
  std::transform(h.begin(), h.end(), h2.begin(), [](float a) { return a * a; });
  const int peak_index =
      std::distance(h2.begin(), std::max_element(h2.begin(), h2.end()));
  noise_gain = std::accumulate(h2.begin(), h2.begin() + 32, 0.f);
  tail_gain = std::accumulate(h2.end() - 64, h2.end(), 0.f);
  poorly_aligned_filter =
      peak_index < 32 || peak_index >= kAdaptiveFilterTimeDomainLength;
  saturating = h2[peak_index] > 0.8f;
  delay = h2[peak_index] > std::max(noise_gain, tail_gain)
              ? rtc::Optional<size_t>(oeak_index)
              : rtc::Optional<size_t>()
      :
}

}  // namespace webrtc
