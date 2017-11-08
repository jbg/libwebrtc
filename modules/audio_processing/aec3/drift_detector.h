/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_DRIFT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_DRIFT_DETECTOR_H_

#include <vector>

#include "api/optional.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class DriftDetector {
 public:
  explicit DriftDetector(size_t memory_size);
  ~DriftDetector();
  void Reset();
  rtc::Optional<float> Update(float time, float value);

 private:
  class DriftEstimator {
   public:
    explicit DriftEstimator(size_t memory_size);
    ~DriftEstimator();
    void Reset();
    rtc::Optional<float> Update(float time, float value);

   private:
    void Compute();

    const size_t memory_size_;
    std::vector<float> t_;
    std::vector<float> v_;
    int last_insert_index_;
    rtc::Optional<float> drift_;
    rtc::Optional<float> drift_std_;
    rtc::Optional<float> last_detected_drift_;
  };

  DriftEstimator estimator_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DriftDetector);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_DRIFT_DETECTOR_H_
