/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/drift_detector.h"

#include <math.h>

namespace webrtc {

DriftDetector::DriftDetector(size_t memory_size) : estimator_(memory_size) {
  Reset();
}
DriftDetector::~DriftDetector() = default;

void DriftDetector::Reset() {
  estimator_.Reset();
}

rtc::Optional<float> DriftDetector::Update(float time, float value) {
  return estimator_.Update(time, value);
}

DriftDetector::DriftEstimator::DriftEstimator(size_t memory_size)
    : memory_size_(memory_size),
      t_(memory_size_, 0.f),
      v_(memory_size_, 0.f),
      last_insert_index_(memory_size_ - 1) {
  Reset();
}
DriftDetector::DriftEstimator::~DriftEstimator() = default;

void DriftDetector::DriftEstimator::Reset() {
  t_.resize(0);
  v_.resize(0);
}

rtc::Optional<float> DriftDetector::DriftEstimator::Update(float time,
                                                           float value) {
  // Insert the new data point.
  last_insert_index_ = (last_insert_index_ + 1) % memory_size_;
  if (t_.size() < memory_size_) {
    t_.resize(t_.size() + 1);
    v_.resize(v_.size() + 1);
  }
  t_[last_insert_index_] = time;
  v_[last_insert_index_] = value;

  Compute();

  if (drift_) {
    RTC_DCHECK(!!drift_std_);
    if (fabs(*drift_) <= 0.02 * (*drift_std_)) {
      last_detected_drift_ = rtc::Optional<float>();
    }
    if (fabs(*drift_) > 1.f * (*drift_std_)) {
      last_detected_drift_ = drift_;
    }
  }
  return last_detected_drift_;
}

void DriftDetector::DriftEstimator::Compute() {
  RTC_DCHECK_EQ(memory_size_, t_.capacity());
  RTC_DCHECK_EQ(memory_size_, v_.capacity());

  // No point in computing drift for too few number of samples.
  if (t_.size() > 10) {
    // Estimate the drift.
    double t_sum = 0.f;
    double v_sum = 0.f;
    size_t i = last_insert_index_;
    for (size_t k = 0; k < t_.size(); ++k) {
      t_sum += t_[i];
      v_sum += v_[i];
      i = i > 0 ? i - 1 : memory_size_ - 1;
    }

    const double t_avg = t_sum / t_.size();
    const double v_avg = v_sum / v_.size();

    double num = 0.f;
    double denom = 0.f;
    for (size_t i = last_insert_index_, k = 0; k < t_.size(); ++k) {
      num += (t_[i] - t_avg) * (v_[i] - v_avg);
      const double tmp = (t_[i] - t_avg);
      denom += tmp * tmp;
      i = i > 0 ? i - 1 : memory_size_ - 1;
    }

    if (denom == 0.f) {
      drift_ = rtc::Optional<float>(0);
      drift_std_ = rtc::Optional<float>(0);
    } else {
      drift_ = rtc::Optional<float>(static_cast<float>(num / denom));
      const double drift = *drift_;
      const double mean = v_avg - drift * t_avg;

      // Compute the standard deviation for the drift.
      num = 0.f;

      size_t i = last_insert_index_;
      for (size_t k = 0; k < t_.size(); ++k) {
        const double tmp = v_[i] - t_[i] * drift - mean;
        num += tmp * tmp;
        i = i > 0 ? i - 1 : memory_size_ - 1;
      }

      RTC_DCHECK_LT(2, t_.size());
      num = num / (t_.size() - 2);
      drift_std_ =
          rtc::Optional<float>(static_cast<float>(sqrt(num) / sqrt(denom)));
    }
  } else {
    drift_ = rtc::Optional<float>(0);
    drift_std_ = rtc::Optional<float>(0);
  }
}

}  // namespace webrtc
