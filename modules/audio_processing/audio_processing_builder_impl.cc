/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_processing.h"

#include <memory>

#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
#include "modules/audio_processing/audio_processing_impl.h"
#include "rtc_base/ref_counted_object.h"
#endif

namespace webrtc {

AudioProcessingBuilder::AudioProcessingBuilder() = default;
AudioProcessingBuilder::~AudioProcessingBuilder() = default;

AudioProcessingBuilder& AudioProcessingBuilder::SetCapturePostProcessing(
    std::unique_ptr<CustomProcessing> capture_post_processing) {
  capture_post_processing_ = std::move(capture_post_processing);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetRenderPreProcessing(
    std::unique_ptr<CustomProcessing> render_pre_processing) {
  render_pre_processing_ = std::move(render_pre_processing);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetCaptureAnalyzer(
    std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
  capture_analyzer_ = std::move(capture_analyzer);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetEchoControlFactory(
    std::unique_ptr<EchoControlFactory> echo_control_factory) {
  echo_control_factory_ = std::move(echo_control_factory);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetEchoDetector(
    rtc::scoped_refptr<EchoDetector> echo_detector) {
  echo_detector_ = std::move(echo_detector);
  return *this;
}

AudioProcessing* AudioProcessingBuilder::Create() {
  webrtc::Config config;
  return Create(config);
}

#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
// Standard implementation.
AudioProcessing* AudioProcessingBuilder::Create(const webrtc::Config& config) {
  AudioProcessingImpl* apm = new rtc::RefCountedObject<AudioProcessingImpl>(
      config, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
  if (apm->Initialize() != AudioProcessing::kNoError) {
    delete apm;
    apm = nullptr;
  }
  return apm;
}
#else
// Implementation returning a null pointer for using when the APM is excluded
// from the build..
AudioProcessing* AudioProcessingBuilder::Create(const webrtc::Config& config) {
  return nullptr;
}
#endif
}  // namespace webrtc
