/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_DOMINANT_NEAREND_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_DOMINANT_NEAREND_DETECTOR_H_

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"

namespace webrtc {
// Class for selecting whether the suppressor is in the nearend or echo state.
class DominantNearendDetector {
 public:
  virtual ~DominantNearendDetector() = default;

  // Create an instance of DominantNearendDetector.
  static DominantNearendDetector* Create(
      const EchoCanceller3Config::Suppressor::DominantNearendDetection& config);

  // Returns whether the current state is the nearend state.
  virtual bool IsNearendState() const = 0;

  // Updates the state selection based on latest spectral estimates.
  virtual void Update(rtc::ArrayView<const float> nearend_spectrum,
                      rtc::ArrayView<const float> residual_echo_spectrum,
                      rtc::ArrayView<const float> comfort_noise_spectrum,
                      bool initial_state) = 0;
};

}  // namespace webrtc

#endif  //  MODULES_AUDIO_PROCESSING_AEC3_DOMINANT_NEAREND_DETECTOR_H_
