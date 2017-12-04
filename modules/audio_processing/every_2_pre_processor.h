/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EVERY_2
#define EVERY_2

#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class Every2 : public PreProcessing {
 public:
  void Initialize(int sample_rate_hz, int num_channels) override;
  // Processes the given capture or render signal.
  void Process(AudioBuffer* audio) override;
  // Returns a string representation of the module state.
  std::string ToString() const override;
};
}  // namespace webrtc

#endif  // EVERY_2
