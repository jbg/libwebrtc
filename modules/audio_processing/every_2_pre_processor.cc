/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/every_2_pre_processor.h"

#include <iostream>

#include "modules/audio_processing/audio_buffer.h"

namespace webrtc {

void Every2::Initialize(int sample_rate_hz, int num_channels) {}

void Every2::Process(AudioBuffer* audio) {
  for (size_t i = 0; i < audio->num_channels(); ++i) {
    for (size_t j = 0; j < audio->num_frames(); ++j) {
      audio->channels_f()[i][j] *= (((j % 2) == 0) ? 0 : 1);
    }
  }
  std::cerr << "doing something!" << std::endl;
}

std::string Every2::ToString() const {
  return "";
}
}  // namespace webrtc
