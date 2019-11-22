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

#include <math.h>

#include "rtc_base/checks.h"

namespace rtc {

ExponentialMovingAverage::ExponentialMovingAverage(int half_time_millis) {
  tau_ = static_cast<double>(half_time_millis) / log(2);
}

void ExponentialMovingAverage::AddSample(int64_t now, int sample) {
  if (!last_observation_timestamp_ms_.has_value()) {
    value_ = sample;
  } else {
    RTC_DCHECK(now > *last_observation_timestamp_ms_);
    int64_t age = now - *last_observation_timestamp_ms_;
    double e = exp(-age / tau_);
    double alpha = e / (1 + e);
    double one_minus_alpha = 1 - alpha;
    double sample_diff = sample - value_;
    double new_value = one_minus_alpha * value_ + alpha * sample;
    double new_variance =
        one_minus_alpha * sample_variance_ + alpha * sample_diff * sample_diff;
    double new_estimator_variance =
        (one_minus_alpha * one_minus_alpha) * estimator_variance_ +
        (alpha * alpha);
    value_ = new_value;
    sample_variance_ = new_variance;
    estimator_variance_ = new_estimator_variance;
  }
  last_observation_timestamp_ms_ = now;
}

double ExponentialMovingAverage::GetConfidenceInterval() const {
  return 1.96 * sqrt(sample_variance_ * estimator_variance_);
}

}  // namespace rtc
