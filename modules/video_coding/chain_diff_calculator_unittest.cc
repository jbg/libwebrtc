/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/chain_diff_calculator.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;

TEST(ChainDiffCalculatorTest, SingleChain) {
  // Simulate  a stream with 2 temporal layer where chain
  // protects temporal layer 0.
  ChainDiffCalculator calculator;
  // Key frame.
  calculator.Reset({true});
  EXPECT_THAT(calculator.From(1, {true}), ElementsAre(0));
  // T1 delta frame.
  EXPECT_THAT(calculator.From(2, {false}), ElementsAre(1));
  // T0 delta frame.
  EXPECT_THAT(calculator.From(3, {true}), ElementsAre(2));
}

TEST(ChainDiffCalculatorTest, TwoChainsFullSvc) {
  // Simulate a full svc stream with 2 spatial and 2 temporal layers.
  // chains are protecting temporal layers 0.
  ChainDiffCalculator calculator;
  // S0 Key frame.
  calculator.Reset({true, true});
  EXPECT_THAT(calculator.From(1, {true, true}), ElementsAre(0, 0));
  // S1 Key frame.
  EXPECT_THAT(calculator.From(2, {false, true}), ElementsAre(1, 1));
  // S0T1 delta frame.
  EXPECT_THAT(calculator.From(3, {false, false}), ElementsAre(2, 1));
  // S1T1 delta frame.
  EXPECT_THAT(calculator.From(4, {false, false}), ElementsAre(3, 2));
  // S0T0 delta frame.
  EXPECT_THAT(calculator.From(5, {true, true}), ElementsAre(4, 3));
  // S1T0 delta frame.
  EXPECT_THAT(calculator.From(6, {false, true}), ElementsAre(1, 1));
}

TEST(ChainDiffCalculatorTest, TwoChainsKSvc) {
  // Simulate a k-svc stream with 2 spatial and 2 temporal layers.
  // chains are protecting temporal layers 0.
  ChainDiffCalculator calculator;
  // S0 Key frame.
  calculator.Reset({true, true});
  EXPECT_THAT(calculator.From(1, {true, true}), ElementsAre(0, 0));
  // S1 Key frame.
  EXPECT_THAT(calculator.From(2, {false, true}), ElementsAre(1, 1));
  // S0T1 delta frame.
  EXPECT_THAT(calculator.From(3, {false, false}), ElementsAre(2, 1));
  // S1T1 delta frame.
  EXPECT_THAT(calculator.From(4, {false, false}), ElementsAre(3, 2));
  // S0T0 delta frame.
  EXPECT_THAT(calculator.From(5, {true, false}), ElementsAre(4, 3));
  // S1T0 delta frame.
  EXPECT_THAT(calculator.From(6, {false, true}), ElementsAre(1, 4));
}

TEST(ChainDiffCalculatorTest, TwoChainsSimulcast) {
  // Simulate a k-svc stream with 2 spatial and 2 temporal layers.
  // chains are protecting temporal layers 0.
  ChainDiffCalculator calculator;
  // S0 Key frame.
  calculator.Reset({true, false});
  EXPECT_THAT(calculator.From(1, {true, false}), ElementsAre(0, 0));
  // S1 Key frame.
  calculator.Reset({false, true});
  EXPECT_THAT(calculator.From(2, {false, true}), ElementsAre(1, 0));
  // S0T1 delta frame.
  EXPECT_THAT(calculator.From(3, {false, false}), ElementsAre(2, 1));
  // S1T1 delta frame.
  EXPECT_THAT(calculator.From(4, {false, false}), ElementsAre(3, 2));
  // S0T0 delta frame.
  EXPECT_THAT(calculator.From(5, {true, false}), ElementsAre(4, 3));
  // S1T0 delta frame.
  EXPECT_THAT(calculator.From(6, {false, true}), ElementsAre(1, 4));
}

}  // namespace
}  // namespace webrtc
