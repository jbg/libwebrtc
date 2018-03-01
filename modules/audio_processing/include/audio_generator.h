/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_GENERATOR_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_GENERATOR_H_

#include <memory>

#include "system_wrappers/include/file_wrapper.h"

namespace webrtc {
// This class can be used as input sink for the APM.
// Generates an infinite audio signal, [-1, 1] floating point values, in frames
// of given, possibly varying dimensions and sample rate.
class AudioGenerator {
 public:
  virtual ~AudioGenerator() {}

  // Generates the next frame of audio.
  virtual void GenerateFrame(float* const* audio,
                             size_t num_channels,
                             size_t channel_size,
                             size_t sample_rate_hz) = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_GENERATOR_H_
