/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_H_
#define MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_H_

#include <memory>
#include <vector>

#include "modules/audio_processing/utility/cascaded_biquad_filter.h"

namespace webrtc {

class AudioBuffer;

// Filters that high
class HighPassFilter {
 public:
  static const CascadedBiQuadFilter::BiQuadCoefficients
      kHighPassFilterCoefficients;
  static const size_t kNumberOfHighPassBiQuads = 1;

  explicit HighPassFilter(size_t num_channels);
  ~HighPassFilter();
  HighPassFilter(const HighPassFilter&) = delete;
  HighPassFilter& operator=(const HighPassFilter&) = delete;

  void Process(AudioBuffer* audio);
  void Reset();
  void Reset(size_t num_channels);

 private:
  std::vector<std::unique_ptr<CascadedBiQuadFilter>> filters_;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_H_
