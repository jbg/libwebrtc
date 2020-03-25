/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"

#include <memory>
#include <utility>

#include "modules/audio_processing/audio_processing_impl.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

AudioProcessingBuilderForTesting::AudioProcessingBuilderForTesting() {}
AudioProcessingBuilderForTesting::~AudioProcessingBuilderForTesting() {}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetCapturePostProcessing(
    std::unique_ptr<CustomProcessing> capture_post_processing) {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  ap_builder_.SetCapturePostProcessing(std::move(capture_post_processing));
#else
  capture_post_processing_ = std::move(capture_post_processing);
#endif
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetRenderPreProcessing(
    std::unique_ptr<CustomProcessing> render_pre_processing) {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  ap_builder_.SetRenderPreProcessing(std::move(render_pre_processing));
#else
  render_pre_processing_ = std::move(render_pre_processing);
#endif
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetCaptureAnalyzer(
    std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  ap_builder_.SetCaptureAnalyzer(std::move(capture_analyzer));
#else
  capture_analyzer_ = std::move(capture_analyzer);
#endif
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetEchoControlFactory(
    std::unique_ptr<EchoControlFactory> echo_control_factory) {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  ap_builder_.SetEchoControlFactory(std::move(echo_control_factory));
#else
  echo_control_factory_ = std::move(echo_control_factory);
#endif
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetEchoDetector(
    rtc::scoped_refptr<EchoDetector> echo_detector) {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  ap_builder_.SetEchoDetector(std::move(echo_detector));
#else
  echo_detector_ = std::move(echo_detector);
#endif
  return *this;
}

AudioProcessing* AudioProcessingBuilderForTesting::Create() {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  return ap_builder_.Create();
#else
  webrtc::Config config;
  return Create(config);
#endif
}

AudioProcessing* AudioProcessingBuilderForTesting::Create(
    const webrtc::Config& config) {
#if !defined(EXCLUDE_AUDIO_PROCESSING_MODULE) || \
    EXCLUDE_AUDIO_PROCESSING_MODULE != 1
  return ap_builder_.Create(config);
#else
  AudioProcessingImpl* apm = new rtc::RefCountedObject<AudioProcessingImpl>(
      config, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
  int error = apm->Initialize();
  RTC_CHECK_EQ(error, AudioProcessing::kNoError);
  return apm;
#endif
}
}  // namespace webrtc
