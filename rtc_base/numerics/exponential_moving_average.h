/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_EXPONENTIAL_MOVING_AVERAGE_H_
#define RTC_BASE_NUMERICS_EXPONENTIAL_MOVING_AVERAGE_H_

#include <stdint.h>
#include "absl/types/optional.h"

namespace rtc {

/**
 * This class implements exponential moving average
 * estimating both value, variance and variance of estimator based on
 * https://en.wikipedia.org/w/index.php?title=Moving_average&section=9#Application_to_measuring_computer_performance
 * with the additions from nisse@ added to
 * https://en.wikipedia.org/wiki/Talk:Moving_average.
 */
class ExponentialMovingAverage {
 public:
  explicit ExponentialMovingAverage(int half_time_millis);

  void AddSample(int64_t now, int value);

  double GetAverage() const { return value_; }
  double GetVariance() const { return sample_variance_; }

  // Compute 95% confidence interval assuming that
  // - variance of samples are normal distributed.
  // - variance of estimator is normal distributed.
  double GetConfidenceInterval() const;

 private:
  double tau_;
  double value_;
  double sample_variance_ = 0;
  double estimator_variance_ = 1;
  absl::optional<int64_t> last_observation_timestamp_ms_;
};

}  // namespace rtc

#endif  // RTC_BASE_NUMERICS_EXPONENTIAL_MOVING_AVERAGE_H_
