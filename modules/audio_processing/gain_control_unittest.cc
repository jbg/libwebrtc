/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 100;

void ProcessOneFrame(int sample_rate_hz,
                     AudioBuffer* render_audio_buffer,
                     AudioBuffer* capture_audio_buffer,
                     GainControlImpl* gain_controller) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    render_audio_buffer->SplitIntoFrequencyBands();
    capture_audio_buffer->SplitIntoFrequencyBands();
  }

  std::vector<int16_t> render_audio;
  GainControlImpl::PackRenderAudioBuffer(*render_audio_buffer, &render_audio);
  gain_controller->ProcessRenderAudio(render_audio);
  gain_controller->AnalyzeCaptureAudio(*capture_audio_buffer);
  gain_controller->ProcessCaptureAudio(capture_audio_buffer, false);

  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_audio_buffer->MergeFrequencyBands();
  }
}

void SetupComponent(int sample_rate_hz,
                    GainControl::Mode mode,
                    int target_level_dbfs,
                    int stream_analog_level,
                    int compression_gain_db,
                    bool enable_limiter,
                    int analog_level_min,
                    int analog_level_max,
                    GainControlImpl* gain_controller) {
  gain_controller->Initialize(1, sample_rate_hz);
  GainControl* gc = static_cast<GainControl*>(gain_controller);
  RTC_DCHECK_EQ(mode, GainControl::Mode::kFixedDigital);
  gc->set_stream_analog_level(stream_analog_level);
  gc->set_target_level_dbfs(target_level_dbfs);
  gc->set_compression_gain_db(compression_gain_db);
  gc->enable_limiter(enable_limiter);
  gc->set_analog_level_limits(analog_level_min, analog_level_max);
}

void RunBitExactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         GainControl::Mode mode,
                         int target_level_dbfs,
                         int stream_analog_level,
                         int compression_gain_db,
                         bool enable_limiter,
                         int analog_level_min,
                         int analog_level_max,
                         rtc::ArrayView<const float> output_reference) {
  GainControlImpl gain_controller;
  SetupComponent(sample_rate_hz, mode, target_level_dbfs, stream_analog_level,
                 compression_gain_db, enable_limiter, analog_level_min,
                 analog_level_max, &gain_controller);

  const int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig render_config(sample_rate_hz, num_channels);
  AudioBuffer render_buffer(
      render_config.sample_rate_hz(), render_config.num_channels(),
      render_config.sample_rate_hz(), 1, render_config.sample_rate_hz(), 1);
  test::InputAudioFile render_file(
      test::GetApmRenderTestVectorFileName(sample_rate_hz));
  std::vector<float> render_input(samples_per_channel * num_channels);

  const StreamConfig capture_config(sample_rate_hz, num_channels);
  AudioBuffer capture_buffer(
      capture_config.sample_rate_hz(), capture_config.num_channels(),
      capture_config.sample_rate_hz(), 1, capture_config.sample_rate_hz(), 1);
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);

  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &render_file, render_input);
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(render_config, render_input, &render_buffer);
    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    ProcessOneFrame(sample_rate_hz, &render_buffer, &capture_buffer,
                    &gain_controller);
  }

  // Extract and verify the test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceeding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  const float kElementErrorBound = 1.0f / 32768.0f;
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kElementErrorBound));
}

}  // namespace

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(GainControlBitExactnessTest,
     Mono16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#else
TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#endif
  constexpr float kOutputReference[] = {-0.011749f, -0.008270f, -0.005219f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(GainControlBitExactnessTest,
     Stereo16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#else
TEST(GainControlBitExactnessTest,
     DISABLED_Stereo16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#endif
  constexpr float kOutputReference[] = {-0.048896f, -0.028479f, -0.050345f,
                                        -0.048896f, -0.028479f, -0.050345f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(GainControlBitExactnessTest,
     Mono32kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#else
TEST(GainControlBitExactnessTest,
     DISABLED_Mono32kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#endif
  constexpr float kOutputReference[] = {-0.018158f, -0.016357f, -0.014832f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(GainControlBitExactnessTest,
     Mono48kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#else
TEST(GainControlBitExactnessTest,
     DISABLED_Mono48kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
#endif
  constexpr float kOutputReference[] = {-0.018158f, -0.016357f, -0.014832f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kOutputReference);
}

}  // namespace webrtc
