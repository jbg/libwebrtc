/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/high_pass_filter.h"

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

// [B,A] = butter(2,100/8000,'high')
const CascadedBiQuadFilter::BiQuadCoefficients
    HighPassFilter::kHighPassFilterCoefficients = {
        {0.97261f, -1.94523f, 0.97261f},
        {-1.94448f, 0.94598f}};

HighPassFilter::HighPassFilter(size_t num_channels) {
  filters_.resize(num_channels);
  for (size_t k = 0; k < filters_.size(); ++k) {
    filters_[k].reset(new CascadedBiQuadFilter(kHighPassFilterCoefficients,
                                               kNumberOfHighPassBiQuads));
  }
}

HighPassFilter::~HighPassFilter() = default;

void HighPassFilter::Process(AudioBuffer* audio) {
  RTC_DCHECK(audio);
  RTC_DCHECK_EQ(filters_.size(), audio->num_channels());
  for (size_t k = 0; k < audio->num_channels(); ++k) {
    rtc::ArrayView<float> channel_data =
        rtc::ArrayView<float>(audio->split_bands(k)[0], 160);
    filters_[k]->Process(channel_data);
  }
}

void HighPassFilter::Reset() {
  for (size_t k = 0; k < filters_.size(); ++k) {
    filters_[k]->Reset();
  }
}

void HighPassFilter::Reset(size_t num_channels) {
  const size_t old_num_channels = filters_.size();
  filters_.resize(num_channels);
  if (filters_.size() < old_num_channels) {
    Reset();
  } else {
    for (size_t k = 0; k < old_num_channels; ++k) {
      filters_[k]->Reset();
    }
    for (size_t k = old_num_channels; k < filters_.size(); ++k) {
      filters_[k].reset(new CascadedBiQuadFilter(kHighPassFilterCoefficients,
                                                 kNumberOfHighPassBiQuads));
    }
  }
}

}  // namespace webrtc
