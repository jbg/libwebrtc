/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor_evaluator.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {

ClippingPredictorEvaluator::ClippingPredictorEvaluator(int history_size)
    : history_size_(history_size) {
  RTC_DCHECK_GT(history_size_, 0);
  Reset();
}

ClippingPredictorEvaluator::~ClippingPredictorEvaluator() = default;

void ClippingPredictorEvaluator::Observe(bool clipping_detected,
                                         bool clipping_predicted) {
  const bool expect_clipping = expect_clipping_counter_ > 0;
  expect_clipping_counter_ = std::max(0, expect_clipping_counter_ - 1);

  if (expect_clipping && clipping_detected) {
    if (!predicted_clipping_observed_) {
      true_positives_++;
    }
    predicted_clipping_observed_ = true;
  } else if (expect_clipping && !clipping_detected) {
    if (expect_clipping_counter_ == 0) {
      // This was the last chance to detect clipping after prediction.
      false_positives_++;
    }
  } else if (!expect_clipping && clipping_detected) {
    false_negatives_++;
  } else {
    true_negatives_++;
  }

  if (clipping_predicted) {
    expect_clipping_counter_ = history_size_;
    predicted_clipping_observed_ = false;
  }
}

void ClippingPredictorEvaluator::Reset() {
  expect_clipping_counter_ = 0;
  predicted_clipping_observed_ = false;
  // Reset metrics.
  true_positives_ = 0;
  true_negatives_ = 0;
  false_positives_ = 0;
  false_negatives_ = 0;
}

}  // namespace webrtc
