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
  capture_post_processing_ = std::move(capture_post_processing);
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetRenderPreProcessing(
    std::unique_ptr<CustomProcessing> render_pre_processing) {
  render_pre_processing_ = std::move(render_pre_processing);
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetCaptureAnalyzer(
    std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
  capture_analyzer_ = std::move(capture_analyzer);
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetEchoControlFactory(
    std::unique_ptr<EchoControlFactory> echo_control_factory) {
  echo_control_factory_ = std::move(echo_control_factory);
  return *this;
}

AudioProcessingBuilderForTesting&
AudioProcessingBuilderForTesting::SetEchoDetector(
    rtc::scoped_refptr<EchoDetector> echo_detector) {
  echo_detector_ = std::move(echo_detector);
  return *this;
}

#if defined(WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE) && \
    WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE == 1

AudioProcessing* AudioProcessingBuilderForTesting::Create() {
  webrtc::Config config;
  return Create(config);
}

AudioProcessing* AudioProcessingBuilderForTesting::Create(
    const webrtc::Config& config) {
  AudioProcessingImpl* apm = new rtc::RefCountedObject<AudioProcessingImpl>(
      config, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
  int error = apm->Initialize();
  RTC_CHECK_EQ(error, AudioProcessing::kNoError);
  return apm;
}

#else

AudioProcessing* AudioProcessingBuilderForTesting::Create() {
  AudioProcessingBuilder builder;
  TransferOwnershipsToBuilder(&builder);
  return builder.Create();
}

AudioProcessing* AudioProcessingBuilderForTesting::Create(
    const webrtc::Config& config) {
  AudioProcessingBuilder builder;
  TransferOwnershipsToBuilder(&builder);
  return builder.Create(config);
}

#endif

void AudioProcessingBuilderForTesting::TransferOwnershipsToBuilder(
    AudioProcessingBuilder* builder) {
  builder->SetCapturePostProcessing(std::move(capture_post_processing_));
  builder->SetRenderPreProcessing(std::move(render_pre_processing_));
  builder->SetCaptureAnalyzer(std::move(capture_analyzer_));
  builder->SetEchoControlFactory(std::move(echo_control_factory_));
  builder->SetEchoDetector(std::move(echo_detector_));
}

}  // namespace webrtc
