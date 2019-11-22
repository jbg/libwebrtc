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
  EXPECT_FLOAT_EQ(value / 2, average.GetAverage());
  EXPECT_FLOAT_EQ(500000, average.GetVariance());
  EXPECT_FLOAT_EQ(980, average.GetConfidenceInterval());  // 500 +/- 980
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
  EXPECT_FLOAT_EQ(103.75, average.GetAverage());
  EXPECT_FLOAT_EQ(359.375, average.GetVariance());
  EXPECT_FLOAT_EQ(21.784689, average.GetConfidenceInterval());  // 103 +/- 22

  average.AddSample(time + 4 * kHalfTime, value);
  average.AddSample(time + 5 * kHalfTime, value);
  average.AddSample(time + 6 * kHalfTime, value);
  average.AddSample(time + 7 * kHalfTime, value);
  EXPECT_FLOAT_EQ(100.23438, average.GetAverage());
  EXPECT_FLOAT_EQ(24.108887, average.GetVariance());
  EXPECT_FLOAT_EQ(5.5566177, average.GetConfidenceInterval());  // 100 +/- 5.6
}

}  // namespace rtc
