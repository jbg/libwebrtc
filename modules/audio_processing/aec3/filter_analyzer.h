/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_

#include <vector>

#include "api/optional.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class FilterAnalyzer {
 public:
  FilterAnalyzer();
  ~FilterAnalyzer();
  void Reset();
  rtc::Optional<float> Update(
      const std::array<float, kAdaptiveFilterTimeDomainLength>& h);

 private:
  DriftEstimator estimator_;
  std::array<float, kAdaptiveFilterTimeDomainLength> h_;
  rtc::Optional<size_t> delay_;
  float noise_gain_;
  float tail_gain_;
  bool poorly_aligned_filter_;
  bool saturating_;
  size_t counter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FilterAnalyzer);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
