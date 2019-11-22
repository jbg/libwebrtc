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

}  // namespace

namespace rtc {

TEST(ExponentialMovingAverageTest, FirstValue) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 1000;
  average.AddSample(time, value);
  EXPECT_FLOAT_EQ(value, average.GetAverage());
  EXPECT_FLOAT_EQ(0, average.GetVariance());
  EXPECT_FLOAT_EQ(0, average.GetConfidenceInterval());
}

TEST(ExponentialMovingAverageTest, Half) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 1000;
  average.AddSample(time, value);
  average.AddSample(time + kHalfTime, 0);
  EXPECT_FLOAT_EQ(666.66669, average.GetAverage());
  EXPECT_FLOAT_EQ(333333.34, average.GetVariance());
  EXPECT_FLOAT_EQ(843.44971, average.GetConfidenceInterval());  // 666 +/- 843
}

TEST(ExponentialMovingAverageTest, Same) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 1000;
  average.AddSample(time, value);
  average.AddSample(time + kHalfTime, value);
  EXPECT_FLOAT_EQ(value, average.GetAverage());
  EXPECT_FLOAT_EQ(0, average.GetVariance());
  EXPECT_FLOAT_EQ(0, average.GetConfidenceInterval());
}

TEST(ExponentialMovingAverageTest, Almost100) {
  ExponentialMovingAverage average(kHalfTime);

  int64_t time = 23;
  constexpr int value = 100;
  average.AddSample(time + 0 * kHalfTime, value - 10);
  average.AddSample(time + 1 * kHalfTime, value + 10);
  average.AddSample(time + 2 * kHalfTime, value - 15);
  average.AddSample(time + 3 * kHalfTime, value + 15);
  EXPECT_FLOAT_EQ(100.18519, average.GetAverage());
  EXPECT_FLOAT_EQ(254.11522, average.GetVariance());
  EXPECT_FLOAT_EQ(16.242046, average.GetConfidenceInterval());  // 100 +/- 16

  average.AddSample(time + 4 * kHalfTime, value);
  average.AddSample(time + 5 * kHalfTime, value);
  average.AddSample(time + 6 * kHalfTime, value);
  average.AddSample(time + 7 * kHalfTime, value);
  EXPECT_FLOAT_EQ(100.03658, average.GetAverage());
  EXPECT_FLOAT_EQ(50.203754, average.GetVariance());
  EXPECT_FLOAT_EQ(6.2530847, average.GetConfidenceInterval());  // 100 +/- 6
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
    EXPECT_FLOAT_EQ(50.034657, average.GetAverage());
    EXPECT_FLOAT_EQ(4996.5342, average.GetVariance());
    EXPECT_FLOAT_EQ(97.966057, average.GetConfidenceInterval());  // 50 +/- 97
  }

  {
    ExponentialMovingAverage average(kHalfTime);
    average.AddSample(time + 0, 0);
    average.AddSample(time + 1, 100);
    EXPECT_FLOAT_EQ(49.965343, average.GetAverage());
    EXPECT_FLOAT_EQ(4996.5342, average.GetVariance());
    EXPECT_FLOAT_EQ(97.966057, average.GetConfidenceInterval());  // 49 +/- 97
  }
}

}  // namespace rtc
