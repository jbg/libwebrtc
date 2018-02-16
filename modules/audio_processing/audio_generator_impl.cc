/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_generator_impl.h"

#include <utility>

#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

std::unique_ptr<AudioGenerator> AudioGenerator::Create() {
  return std::unique_ptr<AudioGenerator>(new AudioGeneratorImpl());
}

std::unique_ptr<AudioGenerator> AudioGenerator::Create(
    std::unique_ptr<FileWrapper> input_audio_file) {
  return std::unique_ptr<AudioGenerator>(
      new AudioGeneratorImpl(std::move(input_audio_file)));
}

AudioGeneratorImpl::AudioGeneratorImpl() {
  // TODO(bugs.webrtc.org/8882) Stub.
  // Fill looped_audio_buffer_ with generated audio.
  buffer_position_ = -1;  // -Wunused-private-field
}

AudioGeneratorImpl::AudioGeneratorImpl(
    std::unique_ptr<FileWrapper> input_audio_file) {
  // TODO(bugs.webrtc.org/8882) Stub.
  // Read audio from file into looped_audio_buffer_.
}

AudioGeneratorImpl::~AudioGeneratorImpl() = default;

void AudioGeneratorImpl::GenerateFrame(float* const* audio,
                                       size_t num_channels,
                                       size_t channel_size,
                                       size_t sample_rate_hz) {
  // TODO(bugs.webrtc.org/8882) Stub.
  // Read from looped_audio_buffer and advance buffer_position_.
  // Resample to output rate.
}

}  // namespace webrtc
