/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_EVALUATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_EVALUATOR_H_

namespace webrtc {

// Counts true/false positives/negatives while observing sequences of flag pairs
// that indicate whether clipping has been detected and/or if clipping is
// predicted - i.e., detection expected to occur in the near future unless the
// input gain is reduced.
class ClippingPredictorEvaluator {
 public:
  // Ctor. `history_size` indicates how long to wait for a call to `Observe()`
  // having `clipping_detected` set to true from the time clipping is predicted.
  explicit ClippingPredictorEvaluator(int history_size);
  ClippingPredictorEvaluator(const ClippingPredictorEvaluator&) = delete;
  ClippingPredictorEvaluator& operator=(const ClippingPredictorEvaluator&) =
      delete;
  ~ClippingPredictorEvaluator();

  // Observes whether clipping has been detected and/or if clipping is
  // predicted. When predicted one or more detections are expected in the next
  // `history_size_` calls of `Observe()`.
  void Observe(bool clipping_detected, bool clipping_predicted);

  // Resets all the metrics.
  void Reset();

  // Metrics getters.
  int true_positives() const { return true_positives_; }
  int true_negatives() const { return true_negatives_; }
  int false_positives() const { return false_positives_; }
  int false_negatives() const { return false_negatives_; }

 private:
  const int history_size_;

  // Remaining number of `Observe()` calls during which a call with
  // `clipping_detected` set to true is expected.
  int expect_clipping_counter_;
  // True if clipping has already been detected after it was predicted.
  bool predicted_clipping_observed_;

  // Metrics.
  int true_positives_;
  int true_negatives_;
  int false_positives_;
  int false_negatives_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_EVALUATOR_H_
