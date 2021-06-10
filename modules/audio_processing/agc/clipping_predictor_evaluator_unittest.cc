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

#include <cstdint>
#include <tuple>

#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr bool kDetected = true;
constexpr bool kNotDetected = false;

constexpr bool kPredicted = true;
constexpr bool kNotPredicted = false;

int SumTrueFalsePositivesNegatives(
    const ClippingPredictorEvaluator& evaluator) {
  return evaluator.true_positives() + evaluator.true_negatives() +
         evaluator.false_positives() + evaluator.false_negatives();
}

TEST(ClippingPredictorEvaluatorTest, Init) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  EXPECT_EQ(evaluator.true_positives(), 0);
  EXPECT_EQ(evaluator.true_negatives(), 0);
  EXPECT_EQ(evaluator.false_positives(), 0);
  EXPECT_EQ(evaluator.false_negatives(), 0);
}

class ClippingPredictorEvaluatorParameterization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  uint64_t seed() const {
    return rtc::checked_cast<uint64_t>(std::get<0>(GetParam()));
  }
  int history_size() const { return std::get<1>(GetParam()); }
};

// Checks that the sum of true/false positives/negatives is not greater than the
// number of calls to `Observe()`.
TEST_P(ClippingPredictorEvaluatorParameterization,
       SumOverMetricsLessEqualThanNumObserveCalls) {
  constexpr int kNumCalls = 123;
  Random random_generator(seed());
  ClippingPredictorEvaluator evaluator(history_size());
  for (int i = 0; i < kNumCalls; ++i) {
    bool clipping_detected = random_generator.Rand<bool>();
    bool clipping_predicted = random_generator.Rand<bool>();
    evaluator.Observe(clipping_detected, clipping_predicted);
  }
  EXPECT_LE(SumTrueFalsePositivesNegatives(evaluator), kNumCalls);
}

// Checks that after each call to `Observe()` at most one metric grows by the
// history size. Such an upper bound is defined by the true positive growth in
// the case in which multiple predictions are matched by a single detection.
TEST_P(ClippingPredictorEvaluatorParameterization,
       AtMostOneMetricGrowsByHistorySize) {
  constexpr int kNumCalls = 123;
  Random random_generator(seed());
  ClippingPredictorEvaluator evaluator(history_size());

  int sum = SumTrueFalsePositivesNegatives(evaluator);
  for (int i = 0; i < kNumCalls; ++i) {
    SCOPED_TRACE(i);
    bool clipping_detected = random_generator.Rand<bool>();
    bool clipping_predicted = random_generator.Rand<bool>();
    evaluator.Observe(clipping_detected, clipping_predicted);

    const int new_sum = SumTrueFalsePositivesNegatives(evaluator);
    const int difference = new_sum - sum;
    EXPECT_GE(difference, 0);
    EXPECT_LE(difference, history_size());
    sum = new_sum;
  }
}

INSTANTIATE_TEST_SUITE_P(
    ClippingPredictorEvaluatorTest,
    ClippingPredictorEvaluatorParameterization,
    ::testing::Combine(::testing::Values(4, 8, 15, 16, 23, 42),
                       ::testing::Values(1, 10, 21)));

// Checks that, when clipping is detected the first time that `Observe()` is
// called, that generates a false negative - i.e., no grace period is applied
// after initialization.
TEST(ClippingPredictorEvaluatorTest, NoGracePeriodAfterInit) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
}

// Checks that `clipping_predicted` predicts the future - i.e., it does not
// apply to the current observation.
TEST(ClippingPredictorEvaluatorTest, PredictDoesNotApplyToCurrentCall) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);

  // First call.
  evaluator.Observe(kDetected, kPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
  evaluator.Reset();

  // Same expectation afterwards.
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kDetected, kPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
}

// Checks that no clipping is expected the first time `Observe()` is called.
TEST(ClippingPredictorEvaluatorTest, ExpectNoClippingAfterInit) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);

  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.true_negatives(), 1);
  evaluator.Reset();

  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_negatives(), 1);
  evaluator.Reset();

  evaluator.Observe(kDetected, kPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
  evaluator.Reset();

  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
}

// Checks that the evaluator detects true negatives when clipping is neither
// predicted nor detected.
TEST(ClippingPredictorEvaluatorTest, NeverDetectedAndNotPredicted) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_negatives(), 4);
}

// Checks that the evaluator detects a false negative when clipping is detected
// but not predicted.
TEST(ClippingPredictorEvaluatorTest, DetectedButNotPredicted) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
}

// Checks that the evaluator detects a false positive when clipping is predicted
// but never detected.
TEST(ClippingPredictorEvaluatorTest, PredictedOnceButNeverDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 1);
}

// Checks that the evaluator does not detect a false positive when clipping is
// predicted but not detected until the observation period expires.
TEST(ClippingPredictorEvaluatorTest,
     PredictedOnceAndNeverDetectedBeforeDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 1);
}

// Checks that the evaluator detects a false positive when clipping is predicted
// but detected after the observation period expires.
TEST(ClippingPredictorEvaluatorTest, PredictedOnceButDetectedAfterDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 1);
}

// Checks that a prediction followed by a detection counts as true positive.
TEST(ClippingPredictorEvaluatorTest, PredictedOnceAndThenImmediatelyDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_positives(), 1);
}

// Checks that a prediction followed by a delayed detection counts as true
// positive if the delay is within the observation period.
TEST(ClippingPredictorEvaluatorTest, PredictedOnceAndDetectedBeforeDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_positives(), 1);
}

// Checks that a prediction followed by a delayed detection counts as true
// positive if the delay equals the observation period.
TEST(ClippingPredictorEvaluatorTest, PredictedOnceAndDetectedAtDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_positives(), 0);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_positives(), 1);
}

// Checks that a prediction followed by a multiple adjacent detections within
// the deadline counts as a single true positive and that, after the deadline,
// a detection counts as a false negative.
TEST(ClippingPredictorEvaluatorTest, PredictedOnceAndDetectedMultipleTimes) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  // Multiple detections.
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_positives(), 1);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_positives(), 1);
  // A detection outside of the observation period counts as false negative.
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.false_negatives(), 1);
}

TEST(ClippingPredictorEvaluatorTest,
     PredictedMultipleTimesAndDetectedOnceAfterDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // ---+
  evaluator.Observe(kNotDetected, kPredicted);  //    |
  evaluator.Observe(kNotDetected, kPredicted);  //    |
  evaluator.Observe(kNotDetected, kPredicted);  // <--+ Not matched.
  // The time to match a detection after the first prediction expired.
  EXPECT_EQ(evaluator.false_positives(), 1);
  evaluator.Observe(kDetected, kNotPredicted);
  // The detection above does not match the first prediction because it happened
  // after the deadline of the 1st prediction.
  EXPECT_EQ(evaluator.false_positives(), 1);
}

TEST(ClippingPredictorEvaluatorTest, PredictedMultipleTimesAndDetectedOnce) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // --+
  evaluator.Observe(kNotDetected, kPredicted);  //   | --+
  evaluator.Observe(kNotDetected, kPredicted);  //   |   | --+
  evaluator.Observe(kDetected, kNotPredicted);  // <-+ <-+ <-+
  EXPECT_EQ(evaluator.true_positives(), 3);
  // The following observations do not generate any true negatives as they
  // belong to the observation period of the last prediction - for which a
  // detection has already been matched.
  const int true_negatives = evaluator.true_negatives();
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_negatives(), true_negatives);
  // No mistakes expected.
  EXPECT_EQ(evaluator.false_positives(), 0);
  EXPECT_EQ(evaluator.false_negatives(), 0);
}

TEST(ClippingPredictorEvaluatorTest, PredictedMultipleTimesAndSomeDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // --+
  evaluator.Observe(kNotDetected, kPredicted);  //   | --+
  evaluator.Observe(kNotDetected, kPredicted);  //   |   | --+
  evaluator.Observe(kDetected, kNotPredicted);  // <-+ <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);  //     <-+ <-+
  EXPECT_EQ(evaluator.true_positives(), 3);
  // The following observation does not generate a true negative as it belongs
  // to the observation period of the last prediction - for which two detections
  // have already been matched.
  const int true_negatives = evaluator.true_negatives();
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.true_negatives(), true_negatives);
  // No mistakes expected.
  EXPECT_EQ(evaluator.false_positives(), 0);
  EXPECT_EQ(evaluator.false_negatives(), 0);
}

TEST(ClippingPredictorEvaluatorTest, PredictedMultipleTimesAndAllDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // --+
  evaluator.Observe(kNotDetected, kPredicted);  //   | --+
  evaluator.Observe(kNotDetected, kPredicted);  //   |   | --+
  evaluator.Observe(kDetected, kNotPredicted);  // <-+ <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);  //     <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);  //         <-+
  EXPECT_EQ(evaluator.true_positives(), 3);
  EXPECT_EQ(evaluator.true_negatives(), 1);  // First `Observe()` call.
  EXPECT_EQ(evaluator.false_positives(), 0);
  EXPECT_EQ(evaluator.false_negatives(), 0);
}

TEST(ClippingPredictorEvaluatorTest,
     PredictedMultipleTimesWithGapAndAllDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);     // --+
  evaluator.Observe(kNotDetected, kNotPredicted);  //   |
  evaluator.Observe(kNotDetected, kPredicted);     //   | --+
  evaluator.Observe(kDetected, kNotPredicted);     // <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);     //     <-+
  evaluator.Observe(kDetected, kNotPredicted);     //     <-+
  EXPECT_EQ(evaluator.true_positives(), 2);
  EXPECT_EQ(evaluator.true_negatives(), 1);  // First `Observe()` call.
  EXPECT_EQ(evaluator.false_positives(), 0);
  EXPECT_EQ(evaluator.false_negatives(), 0);
}

}  // namespace
}  // namespace webrtc
