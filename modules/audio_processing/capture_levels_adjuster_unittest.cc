/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_levels_adjuster.h"

#include <algorithm>
#include <tuple>

#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr float kSampleValue = 100.f;

float ComputeExpectedSignalGainAfterPreLevelAdjustment(
    bool emulated_analog_mic_gain_enabled,
    int emulated_analog_mic_gain_level,
    float pre_gain) {
  if (!emulated_analog_mic_gain_enabled) {
    return pre_gain;
  }
  return pre_gain * std::min(emulated_analog_mic_gain_level, 255) / 255.f;
}

float ComputeExpectedSignalGainAfterPostLevelAdjustment(
    bool emulated_analog_mic_gain_enabled,
    int emulated_analog_mic_gain_level,
    float pre_gain,
    float post_gain) {
  return post_gain * ComputeExpectedSignalGainAfterPreLevelAdjustment(
                         emulated_analog_mic_gain_enabled,
                         emulated_analog_mic_gain_level, pre_gain);
}

}  // namespace

class CaptureLevelsAdjusterTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<int, int, bool, int, float, float>> {};

INSTANTIATE_TEST_SUITE_P(
    CaptureLevelsAdjusterTestSuite,
    CaptureLevelsAdjusterTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2, 4),
                       ::testing::Values(false, true),
                       ::testing::Values(21, 255),
                       ::testing::Values(0.1f, 1.f, 4.f),
                       ::testing::Values(0.1f, 1.f, 4.f)));

TEST_P(CaptureLevelsAdjusterTest, InitialGainIsInstantlyAchieved) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_channels = std::get<1>(GetParam());
  const int emulated_analog_mic_gain_enabled = std::get<2>(GetParam());
  const int emulated_analog_mic_gain_level = std::get<3>(GetParam());
  const float pre_gain = std::get<4>(GetParam());
  const float post_gain = std::get<5>(GetParam());

  CaptureLevelsAdjuster adjuster(emulated_analog_mic_gain_enabled,
                                 emulated_analog_mic_gain_level, pre_gain,
                                 post_gain);

  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);

  const float expected_signal_gain_after_pre_gain =
      ComputeExpectedSignalGainAfterPreLevelAdjustment(
          emulated_analog_mic_gain_enabled, emulated_analog_mic_gain_level,
          pre_gain);
  const float expected_signal_gain_after_post_level_adjustment =
      ComputeExpectedSignalGainAfterPostLevelAdjustment(
          emulated_analog_mic_gain_enabled, emulated_analog_mic_gain_level,
          pre_gain, post_gain);

  constexpr int kNumFramesToProcess = 10;
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    test::FillBuffer(kSampleValue, audio_buffer);
    adjuster.PreLevelAdjustment(audio_buffer);
    EXPECT_FLOAT_EQ(adjuster.GetPreAdjustmentGain(),
                    expected_signal_gain_after_pre_gain);

    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        expected_signal_gain_after_pre_gain * kSampleValue);
      }
    }
    adjuster.PostLevelAdjustment(audio_buffer);
    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(
            audio_buffer.channels_const()[ch][i],
            expected_signal_gain_after_post_level_adjustment * kSampleValue);
      }
    }
  }
}

TEST_P(CaptureLevelsAdjusterTest, NewGainAreAchieved) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_channels = std::get<1>(GetParam());
  const int emulated_analog_mic_gain_enabled = std::get<2>(GetParam());
  const int lower_emulated_analog_mic_gain_level = std::get<3>(GetParam());
  const float lower_pre_gain = std::get<4>(GetParam());
  const float lower_post_gain = std::get<5>(GetParam());
  const int higher_emulated_analog_mic_gain_level =
      std::min(lower_emulated_analog_mic_gain_level * 2, 255);
  const float higher_pre_gain = lower_pre_gain * 2.f;
  const float higher_post_gain = lower_post_gain * 2.f;

  CaptureLevelsAdjuster adjuster(emulated_analog_mic_gain_enabled,
                                 lower_emulated_analog_mic_gain_level,
                                 lower_pre_gain, lower_post_gain);

  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);

  const float expected_signal_gain_after_pre_gain =
      ComputeExpectedSignalGainAfterPreLevelAdjustment(
          emulated_analog_mic_gain_enabled,
          higher_emulated_analog_mic_gain_level, higher_pre_gain);
  const float expected_signal_gain_after_post_level_adjustment =
      ComputeExpectedSignalGainAfterPostLevelAdjustment(
          emulated_analog_mic_gain_enabled,
          higher_emulated_analog_mic_gain_level, higher_pre_gain,
          higher_post_gain);

  adjuster.SetPreGain(higher_pre_gain);
  adjuster.SetPostGain(higher_post_gain);
  adjuster.SetAnalogMicGainLevel(higher_emulated_analog_mic_gain_level);

  constexpr int kNumFramesToProcess = 10;
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    test::FillBuffer(kSampleValue, audio_buffer);

    if (frame == 0) {
      adjuster.PreLevelAdjustment(audio_buffer);
      EXPECT_FLOAT_EQ(adjuster.GetPreAdjustmentGain(),
                      expected_signal_gain_after_pre_gain);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
          EXPECT_LT(audio_buffer.channels_const()[ch][i],
                    expected_signal_gain_after_pre_gain * kSampleValue);
        }
      }

      adjuster.PostLevelAdjustment(audio_buffer);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
          EXPECT_LT(
              audio_buffer.channels_const()[ch][i],
              expected_signal_gain_after_post_level_adjustment * kSampleValue);
        }
      }

    } else {
      adjuster.PreLevelAdjustment(audio_buffer);
      EXPECT_FLOAT_EQ(adjuster.GetPreAdjustmentGain(),
                      expected_signal_gain_after_pre_gain);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
          EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                          expected_signal_gain_after_pre_gain * kSampleValue);
        }
      }

      adjuster.PostLevelAdjustment(audio_buffer);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
          EXPECT_FLOAT_EQ(
              audio_buffer.channels_const()[ch][i],
              expected_signal_gain_after_post_level_adjustment * kSampleValue);
        }
      }
    }

    EXPECT_EQ(adjuster.GetAnalogMicGainLevel(),
              higher_emulated_analog_mic_gain_level);
  }
}

}  // namespace webrtc
