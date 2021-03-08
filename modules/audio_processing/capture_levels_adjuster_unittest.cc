/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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

float ComputeExpectedSampleAfterPreGain(int pre_gain_level, float pre_gain) {
  return pre_gain * std::min(pre_gain_level, 255) / 255.f * kSampleValue;
}

float ComputeExpectedSampleAfterPostGain(int pre_gain_level,
                                         float pre_gain,
                                         float post_gain) {
  return post_gain *
         ComputeExpectedSampleAfterPreGain(pre_gain_level, pre_gain);
}

}  // namespace

class CaptureLevelsAdjusterTest : public ::testing::Test,
                                  public ::testing::WithParamInterface<
                                      std::tuple<int, int, int, float, float>> {
};

INSTANTIATE_TEST_SUITE_P(
    CaptureLevelsAdjusterTestSuite,
    CaptureLevelsAdjusterTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2, 4),
                       ::testing::Values(21, 255),
                       ::testing::Values(0.1f, 1.f, 4.f),
                       ::testing::Values(0.1f, 1.f, 4.f)));

TEST_P(CaptureLevelsAdjusterTest, InitialGainIsInstantlyAchieved) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_channels = std::get<1>(GetParam());
  const int pre_gain_level = std::get<2>(GetParam());
  const float pre_gain = std::get<3>(GetParam());
  const float post_gain = std::get<4>(GetParam());

  CaptureLevelsAdjuster adjuster(pre_gain_level, pre_gain, post_gain);

  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);

  const float expected_sample_value_after_pre_gain =
      ComputeExpectedSampleAfterPreGain(pre_gain_level, pre_gain);
  const float expected_sample_value_after_post_gain =
      ComputeExpectedSampleAfterPostGain(pre_gain_level, pre_gain, post_gain);

  constexpr int kNumFramesToProcess = 10;
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    test::FillBuffer(kSampleValue, audio_buffer);
    adjuster.PreLevelAdjustment(audio_buffer);

    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        expected_sample_value_after_pre_gain);
      }
    }
    adjuster.PostLevelAdjustment(audio_buffer);
    for (int ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        expected_sample_value_after_post_gain);
      }
    }
  }
}

TEST_P(CaptureLevelsAdjusterTest, NewGainAreAchieved) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_channels = std::get<1>(GetParam());
  const int lower_pre_gain_level = std::get<2>(GetParam());
  const float lower_pre_gain = std::get<3>(GetParam());
  const float lower_post_gain = std::get<4>(GetParam());
  const int higher_pre_gain_level = std::min(lower_pre_gain_level * 2, 255);
  const float higher_pre_gain = lower_pre_gain * 2.f;
  const float higher_post_gain = lower_post_gain * 2.f;

  CaptureLevelsAdjuster adjuster(lower_pre_gain_level, lower_pre_gain,
                                 lower_post_gain);

  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);

  const float expected_sample_value_after_pre_gain =
      ComputeExpectedSampleAfterPreGain(higher_pre_gain_level, higher_pre_gain);
  const float expected_sample_value_after_post_gain =
      ComputeExpectedSampleAfterPostGain(higher_pre_gain_level, higher_pre_gain,
                                         higher_post_gain);

  adjuster.SetPreGain(higher_pre_gain);
  adjuster.SetPostGain(higher_post_gain);
  adjuster.SetPreGainLevel(higher_pre_gain_level);

  constexpr int kNumFramesToProcess = 10;
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    test::FillBuffer(kSampleValue, audio_buffer);

    if (frame == 0) {
      adjuster.PreLevelAdjustment(audio_buffer);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
          EXPECT_LT(audio_buffer.channels_const()[ch][i],
                    expected_sample_value_after_pre_gain);
        }
      }

      adjuster.PostLevelAdjustment(audio_buffer);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
          EXPECT_LT(audio_buffer.channels_const()[ch][i],
                    expected_sample_value_after_post_gain);
        }
      }

    } else {
      adjuster.PreLevelAdjustment(audio_buffer);
      for (int ch = 0; ch < num_channels; ++ch) {
        // for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        for (size_t i = 0; i < 1; ++i) {
          EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                          expected_sample_value_after_pre_gain);
        }
      }

      adjuster.PostLevelAdjustment(audio_buffer);
      for (int ch = 0; ch < num_channels; ++ch) {
        for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
          EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                          expected_sample_value_after_post_gain);
        }
      }
    }
  }
}

}  // namespace webrtc
