/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/speech_probability_buffer.h"

#include <algorithm>

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr float kLowProbabilityThreshold = 0.2f;
constexpr float kAbsError = 0.001f;

class SpeechProbabilityBufferParametrizedTest
    : public ::testing::TestWithParam<int> {
 protected:
  int GetCapacity() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         SpeechProbabilityBufferParametrizedTest,
                         ::testing::Values(-1, 0, 1, 5, 100, 123));

TEST_P(SpeechProbabilityBufferParametrizedTest, CheckNoUpdates) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest,
       CheckUpdatesBelowBufferCapacity) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  for (int i = 0; i < buffer.GetBufferCapacity() / 2; ++i) {
    buffer.Update(0.7f);
  }

  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(buffer.GetSumProbabilities(),
              0.7f * std::max(GetCapacity() / 2, 0), kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest,
       CheckUpdatesEqualToBufferCapacity) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  for (int i = 0; i < buffer.GetBufferCapacity(); ++i) {
    buffer.Update(0.7f);
  }

  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.7f * std::max(GetCapacity(), 0),
              kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest,
       CheckUpdatesBeyondBufferCapacity) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  for (int i = 0; i < 2 * buffer.GetBufferCapacity(); ++i) {
    buffer.Update(0.7f);
  }

  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.7f * std::max(GetCapacity(), 0),
              kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest, CheckReset) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);
  buffer.Update(0.7f);
  buffer.Update(0.7f);

  buffer.Reset();

  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest, CheckLowProbability) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  buffer.Update(0.1f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest,
       CheckNoTransientRemovedAfterManyHighProbabilities) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  // Fill the buffer with low probability followed by a few high probabilities.
  for (int i = 0; i < 20; ++i) {
    buffer.Update(0.9f);
  }

  const float probability = 0.9f;

  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(
      buffer.GetSumProbabilities(),
      probability * std::max(std::min(20, buffer.GetBufferCapacity()), 1),
      kAbsError);

  buffer.Update(0.0f);

  // Expect no transient removal to after several high probabilities.
  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(
      buffer.GetSumProbabilities(),
      probability * std::max(std::min(20, buffer.GetBufferCapacity() - 1), 0),
      kAbsError);

  buffer.Update(0.7f);

  // Expect no transient removal to after several high probabilities.
  EXPECT_EQ(buffer.GetBufferCapacity(), std::max(GetCapacity(), 0));
  EXPECT_NEAR(
      buffer.GetSumProbabilities(),
      0.7f + probability *
                 std::max(std::min(20, buffer.GetBufferCapacity() - 2), 0),
      kAbsError);
}

TEST_P(SpeechProbabilityBufferParametrizedTest,
       CheckTransientRemovedAfterFewHighProbabilities) {
  SpeechProbabilityBuffer buffer(GetCapacity(), kLowProbabilityThreshold);

  // Fill the buffer with low probability followed by a few high probabilities.
  for (int i = 0; i < 14; ++i) {
    buffer.Update(0.1f);
  }
  for (int i = 0; i < 6; ++i) {
    buffer.Update(0.9f);
  }

  EXPECT_NEAR(buffer.GetSumProbabilities(),
              0.9f * std::max(std::min(GetCapacity(), 6), 1), kAbsError);

  buffer.Update(0.0f);

  // Expect transient removal to remove the high probabilities.
  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);
}

TEST(SpeechProbabilityBufferTest, CheckMetricsAfterUpdateBeyondCapacity) {
  SpeechProbabilityBuffer buffer(/*capacity=*/2, kLowProbabilityThreshold);

  buffer.Update(0.3f);
  buffer.Update(0.4f);
  buffer.Update(0.5f);
  buffer.Update(0.7f);
  buffer.Update(0.6f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 1.3f, kAbsError);
}

TEST(SpeechProbabilityBufferTest, CheckMetricsAfterFewUpdates) {
  SpeechProbabilityBuffer buffer(/*capacity=*/4, kLowProbabilityThreshold);

  buffer.Update(0.1f);
  buffer.Update(0.3f);
  buffer.Update(0.25f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.55f, kAbsError);
}

TEST(SpeechProbabilityBufferTest, CheckMetricsAfterReset) {
  SpeechProbabilityBuffer buffer(/*capacity=*/2, kLowProbabilityThreshold);

  buffer.Update(0.4f);
  buffer.Reset();
  buffer.Update(0.5f);
  buffer.Update(0.7f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 1.2f, kAbsError);
}

TEST(SpeechProbabilityBufferTest,
     CheckMetricsAfterTransientRemovalBeyondCapacity) {
  SpeechProbabilityBuffer buffer(/*capacity=*/5, kLowProbabilityThreshold);

  buffer.Update(0.0f);
  buffer.Update(0.0f);
  buffer.Update(0.4f);
  buffer.Update(0.4f);
  buffer.Update(0.4f);
  buffer.Update(0.0f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);
}

TEST(SpeechProbabilityBufferTest,
     CheckMetricsAfterTransientRemovalAfterFewUpdates) {
  SpeechProbabilityBuffer buffer(/*capacity=*/8, kLowProbabilityThreshold);

  buffer.Update(0.4f);
  buffer.Update(0.4f);
  buffer.Update(0.0f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);
}

TEST(SpeechProbabilityBufferTest, CheckMetricsAfterNoTransientRemoved) {
  SpeechProbabilityBuffer buffer(/*capacity=*/10, kLowProbabilityThreshold);

  buffer.Update(0.4f);
  buffer.Update(0.5f);
  buffer.Update(0.6f);
  buffer.Update(0.7f);
  buffer.Update(0.8f);
  buffer.Update(0.9f);
  buffer.Update(1.0f);
  buffer.Update(0.9f);
  buffer.Update(0.8f);
  buffer.Update(0.7f);
  buffer.Update(0.6f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 7.5f, kAbsError);

  buffer.Update(0.0f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 7.0f, kAbsError);

  buffer.Update(0.7f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 7.1f, kAbsError);
}

TEST(SpeechProbabilityBufferTest, CheckMetricsAfterTransientRemoved) {
  SpeechProbabilityBuffer buffer(/*capacity=*/10, kLowProbabilityThreshold);

  buffer.Update(0.1f);
  buffer.Update(0.1f);
  buffer.Update(0.1f);
  buffer.Update(0.1f);
  buffer.Update(0.1f);
  buffer.Update(0.1f);
  buffer.Update(0.1f);
  buffer.Update(0.7f);
  buffer.Update(0.8f);
  buffer.Update(0.9f);
  buffer.Update(1.0f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 3.4f, kAbsError);

  buffer.Update(0.0f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.0f, kAbsError);

  buffer.Update(0.7f);

  EXPECT_NEAR(buffer.GetSumProbabilities(), 0.7f, kAbsError);
}

}  // namespace
}  // namespace webrtc
