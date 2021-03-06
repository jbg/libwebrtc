/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/audio_samples_scaler.h"

#include <tuple>

#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "test/gtest.h"

namespace webrtc {

class AudioSamplesScalerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int, float>> {};

INSTANTIATE_TEST_SUITE_P(
    AudioSamplesScalerTestSuite,
    AudioSamplesScalerTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2, 4),
                       ::testing::Values(0.1f, 1.f, 2.f, 4.f)));

TEST_P(AudioSamplesScalerTest, InitialGainIsRespected) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_channels = std::get<1>(GetParam());
  const float initial_gain = std::get<2>(GetParam());

  AudioSamplesScaler scaler(initial_gain);

  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);
  constexpr int kNumFramesToProcess = 10;
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    constexpr float kSampleValue = 100.f;
    test::FillBuffer(kSampleValue, audio_buffer);
    scaler.Process(audio_buffer);
    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        initial_gain * kSampleValue);
      }
    }
  }
}

TEST_P(AudioSamplesScalerTest, VerifyGainAdjustment) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_channels = std::get<1>(GetParam());
  const float higher_gain = std::get<2>(GetParam());
  const float lower_gain = higher_gain / 2.f;

  AudioSamplesScaler scaler(lower_gain);

  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);

  constexpr int kNumFramesToProcess = 10;
  constexpr float kSampleValue = 100.f;

  // Allow the intial, lower, gain to take effect.
  test::FillBuffer(kSampleValue, audio_buffer);
  scaler.Process(audio_buffer);

  // Set the new, higher, gain.
  scaler.SetGain(higher_gain);

  // Ensure that the new, higher, gain is achieved gradually over one frame.
  test::FillBuffer(kSampleValue, audio_buffer);
  scaler.Process(audio_buffer);
  for (int ch = 0; ch < num_channels; ++ch) {
    for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
      EXPECT_LT(audio_buffer.channels_const()[ch][i],
                higher_gain * kSampleValue);
    }
  }

  // Ensure that the new, higher, gain is achieved and stay unchanged.
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    test::FillBuffer(kSampleValue, audio_buffer);
    scaler.Process(audio_buffer);

    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        higher_gain * kSampleValue);
      }
    }
  }

  // Set the new, lower, gain.
  scaler.SetGain(lower_gain);

  // Ensure that the new, lower, gain is achieved gradually over one frame.
  test::FillBuffer(kSampleValue, audio_buffer);
  scaler.Process(audio_buffer);
  for (int ch = 0; ch < num_channels; ++ch) {
    for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
      EXPECT_GT(audio_buffer.channels_const()[ch][i],
                lower_gain * kSampleValue);
    }
  }

  // Ensure that the new, lower, gain is achieved and stay unchanged.
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    test::FillBuffer(kSampleValue, audio_buffer);
    scaler.Process(audio_buffer);

    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        lower_gain * kSampleValue);
      }
    }
  }
}

}  // namespace webrtc
