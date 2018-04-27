/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_VAD_VAD_WITH_LEVEL_H_
#define MODULES_AUDIO_PROCESSING_VAD_VAD_WITH_LEVEL_H_

#include <array>

#include "api/array_view.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/vad/voice_activity_detector.h"

namespace webrtc {
class VadWithLevel {
 public:
  // TODO(webrtc:7494): This is a stub. Add implementation.
  rtc::ArrayView<const VoiceActivityDetector::LevelAndProbability> AnalyzeFrame(
      AudioFrameView<const float> frame) {
    // First attempt: we only feed the first channel to the VAD. The VAD
    // takes int16 values. We convert the first channel to int16.
    std::array<int16_t, 480> first_channel_as_int;
    RTC_DCHECK_LE(frame.samples_per_channel(), 480);
    std::transform(frame.channel(0).begin(),
                   frame.channel(0).begin() + frame.samples_per_channel(),
                   first_channel_as_int.begin(),
                   [](float f) { return static_cast<int16_t>(f); });

    vad_.ProcessChunk(&first_channel_as_int[0], frame.samples_per_channel(),
                      static_cast<int>(frame.samples_per_channel() * 100));

    return vad_.levels_and_probability();
  }

 private:
  VoiceActivityDetector vad_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_VAD_VAD_WITH_LEVEL_H_
