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
  decay_factor_ = static_cast<double>(half_time_millis) / log(2);
}

void ExponentialMovingAverage::AddSample(int64_t now, int sample) {
  if (n == 0) {
    value_ = sample;
    sample_variance_ = 0;
    estimator_variance_ = 1;
  } else {
    RTC_DCHECK(now > last_observation_timestamp_ms_);
    int64_t age = now - last_observation_timestamp_ms_;
    double alpha = 1 - exp(-age / decay_factor_);
    double one_minus_alpha = 1 - alpha;
    double new_value = alpha * value_ + one_minus_alpha * sample;
    double new_variance = alpha * sample_variance_ + one_minus_alpha *
                                                         (value_ - sample) *
                                                         (value_ - sample);
    double new_estimator_variance = (alpha * alpha) * estimator_variance_ +
                                    (one_minus_alpha * one_minus_alpha);
    value_ = new_value;
    sample_variance_ = new_variance;
    estimator_variance_ = new_estimator_variance;
  }
  n++;
  last_observation_timestamp_ms_ = now;
}

double ExponentialMovingAverage::GetConfidenceInterval() const {
  return 1.96 * sqrt(sample_variance_ * estimator_variance_);
}

}  // namespace rtc
