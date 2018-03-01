/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_generator_factory.h"

#include <utility>

#include "modules/audio_processing/audio_generator/audio_generator_impl.h"

namespace webrtc {

std::unique_ptr<AudioGenerator> AudioGeneratorFactory::Create() {
  return std::unique_ptr<AudioGenerator>(new AudioGeneratorImpl());
}

std::unique_ptr<AudioGenerator> AudioGeneratorFactory::Create(
    std::unique_ptr<FileWrapper> input_audio_file) {
  return std::unique_ptr<AudioGenerator>(
      new AudioGeneratorImpl(std::move(input_audio_file)));
}

}  // namespace webrtc
