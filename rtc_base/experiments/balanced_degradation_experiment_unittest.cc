/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/balanced_degradation_experiment.h"

#include <limits>

#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {
namespace {
std::vector<BalancedDegradationExperiment::Config> DefaultConfig() {
  return {{320 * 240, 7}, {480 * 270, 10}, {640 * 480, 15}};
}
}  // namespace

TEST(BalancedDegradationExperiment, DisabledWithoutFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials("");
  EXPECT_FALSE(BalancedDegradationExperiment::Enabled());
}

TEST(BalancedDegradationExperiment, EnabledWithFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1,2/");
  EXPECT_TRUE(BalancedDegradationExperiment::Enabled());
}

TEST(BalancedDegradationExperiment, GetsConfig) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,7,2000,15,3000,25/");

  const std::vector<BalancedDegradationExperiment::Config> kConfigs =
      BalancedDegradationExperiment::GetConfigs();
  EXPECT_THAT(kConfigs, ::testing::ElementsAre(
                            BalancedDegradationExperiment::Config{1000, 7},
                            BalancedDegradationExperiment::Config{2000, 15},
                            BalancedDegradationExperiment::Config{3000, 25}));
}

TEST(BalancedDegradationExperiment, GetsDefaultConfigIfNotEnabled) {
  EXPECT_THAT(BalancedDegradationExperiment::GetConfigs(),
              ::testing::ElementsAreArray(DefaultConfig()));
}

TEST(BalancedDegradationExperiment, GetsDefaultConfigForTooFewParameters) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,7,2000,15,3000/");
  EXPECT_THAT(BalancedDegradationExperiment::GetConfigs(),
              ::testing::ElementsAreArray(DefaultConfig()));
}

TEST(BalancedDegradationExperiment, GetsMinFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,7,2000,15,3000,25/");

  const std::vector<BalancedDegradationExperiment::Config> kConfigs =
      BalancedDegradationExperiment::GetConfigs();
  ASSERT_EQ(3u, kConfigs.size());
  EXPECT_EQ(7, BalancedDegradationExperiment::MinFps(1, kConfigs));
  EXPECT_EQ(7, BalancedDegradationExperiment::MinFps(1000, kConfigs));
  EXPECT_EQ(15, BalancedDegradationExperiment::MinFps(1000 + 1, kConfigs));
  EXPECT_EQ(15, BalancedDegradationExperiment::MinFps(2000, kConfigs));
  EXPECT_EQ(25, BalancedDegradationExperiment::MinFps(2000 + 1, kConfigs));
  EXPECT_EQ(25, BalancedDegradationExperiment::MinFps(3000, kConfigs));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            BalancedDegradationExperiment::MinFps(3000 + 1, kConfigs));
}

TEST(BalancedDegradationExperiment, GetsMaxFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,7,2000,15,3000,25/");

  const std::vector<BalancedDegradationExperiment::Config> kConfigs =
      BalancedDegradationExperiment::GetConfigs();
  ASSERT_EQ(3u, kConfigs.size());
  EXPECT_EQ(15, BalancedDegradationExperiment::MaxFps(1, kConfigs));
  EXPECT_EQ(15, BalancedDegradationExperiment::MaxFps(1000, kConfigs));
  EXPECT_EQ(25, BalancedDegradationExperiment::MaxFps(1000 + 1, kConfigs));
  EXPECT_EQ(25, BalancedDegradationExperiment::MaxFps(2000, kConfigs));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            BalancedDegradationExperiment::MaxFps(2000 + 1, kConfigs));
}

TEST(BalancedDegradationExperiment, GetsDefaultConfigForZeroFpsValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,0,2000,15,3000,25/");
  EXPECT_THAT(BalancedDegradationExperiment::GetConfigs(),
              ::testing::ElementsAreArray(DefaultConfig()));
}

TEST(BalancedDegradationExperiment, GetsDefaultConfigIfPixelsDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,7,999,15,3000,25/");
  EXPECT_THAT(BalancedDegradationExperiment::GetConfigs(),
              ::testing::ElementsAreArray(DefaultConfig()));
}

TEST(BalancedDegradationExperiment, GetsDefaultConfigIfFramerateDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradation/Enabled-1000,7,2000,6,3000,25/");
  EXPECT_THAT(BalancedDegradationExperiment::GetConfigs(),
              ::testing::ElementsAreArray(DefaultConfig()));
}

}  // namespace webrtc
