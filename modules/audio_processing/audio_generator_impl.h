/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AUDIO_GENERATOR_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AUDIO_GENERATOR_IMPL_H_

#include <memory>
#include <vector>

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructormagic.h"
#include "system_wrappers/include/file_wrapper.h"

namespace webrtc {

// An AudioGeneratorImpl generates an infinite audio signal, [-1, 1] floating
// point values, in frames of given dimensions and sample rate.
class AudioGeneratorImpl : public AudioGenerator {
 public:
  // Generates a predefined sequence of tone sweep and noise.
  AudioGeneratorImpl();
  // Reads the playout audio from a given WAV file.
  explicit AudioGeneratorImpl(std::unique_ptr<FileWrapper> input_audio_file);

  ~AudioGeneratorImpl() override;

  // Generates the next frame of audio.
  void GenerateFrame(float* const* audio,
                     size_t num_channels,
                     size_t channel_size,
                     size_t sample_rate_hz) override;

 private:
  size_t buffer_position_ = 0;
  std::vector<std::vector<float>> looped_audio_buffer_;
  std::vector<std::vector<float>> audio_frame_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioGeneratorImpl);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AUDIO_GENERATOR_IMPL_H_
