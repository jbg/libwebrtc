/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/exponential_moving_average.h"

#include <cmath>

#include "test/gtest.h"

namespace {

constexpr int kHalfTime = 500;
constexpr double kError = 0.1;

}  // namespace

namespace rtc {

TEST(ExponentialMovingAverageTest, FirstValue) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 1000;
  average.AddSample(time, value);
  EXPECT_NEAR(value, average.GetAverage(), kError);
  EXPECT_NEAR(0, average.GetVariance(), kError);
  EXPECT_NEAR(0, average.GetConfidenceInterval(), kError);
}

TEST(ExponentialMovingAverageTest, Half) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 1000;
  average.AddSample(time, value);
  average.AddSample(time + kHalfTime, 0);
  EXPECT_NEAR(666.7, average.GetAverage(), kError);
  EXPECT_NEAR(333333.3, average.GetVariance(), kError);
  EXPECT_NEAR(843.4, average.GetConfidenceInterval(), kError);  // 666 +/- 843
}

TEST(ExponentialMovingAverageTest, Same) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 1000;
  average.AddSample(time, value);
  average.AddSample(time + kHalfTime, value);
  EXPECT_NEAR(value, average.GetAverage(), kError);
  EXPECT_NEAR(0, average.GetVariance(), kError);
  EXPECT_NEAR(0, average.GetConfidenceInterval(), kError);
}

TEST(ExponentialMovingAverageTest, Almost100) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 100;
  average.AddSample(time + 0 * kHalfTime, value - 10);
  average.AddSample(time + 1 * kHalfTime, value + 10);
  average.AddSample(time + 2 * kHalfTime, value - 15);
  average.AddSample(time + 3 * kHalfTime, value + 15);
  EXPECT_NEAR(100.2, average.GetAverage(), kError);
  EXPECT_NEAR(254.1, average.GetVariance(), kError);
  EXPECT_NEAR(16.2, average.GetConfidenceInterval(), kError);  // 100 +/- 16

  average.AddSample(time + 4 * kHalfTime, value);
  average.AddSample(time + 5 * kHalfTime, value);
  average.AddSample(time + 6 * kHalfTime, value);
  average.AddSample(time + 7 * kHalfTime, value);
  EXPECT_NEAR(100.0, average.GetAverage(), kError);
  EXPECT_NEAR(50.2, average.GetVariance(), kError);
  EXPECT_NEAR(6.3, average.GetConfidenceInterval(), kError);  // 100 +/- 6
}

// Test that getting a value at X and another at X+1
// is almost the same as getting another at X and a value at X+1.
TEST(ExponentialMovingAverageTest, SameTime) {
  int64_t time = 23;
  constexpr int value = 100;

  {
    ExponentialMovingAverage average(kHalfTime);
    average.AddSample(time + 0, value);
    average.AddSample(time + 1, 0);
    EXPECT_NEAR(50, average.GetAverage(), kError);
    EXPECT_NEAR(4996.5, average.GetVariance(), kError);
    EXPECT_NEAR(98, average.GetConfidenceInterval(), kError);  // 50 +/- 97
  }

  {
    ExponentialMovingAverage average(kHalfTime);
    average.AddSample(time + 0, 0);
    average.AddSample(time + 1, 100);
    EXPECT_NEAR(50, average.GetAverage(), kError);
    EXPECT_NEAR(4996.5, average.GetVariance(), kError);
    EXPECT_NEAR(98, average.GetConfidenceInterval(), kError);  // 50 +/- 97
  }
}

}  // namespace rtc
