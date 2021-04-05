/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_timeout.h"

#include "net/dcsctp/public/dcsctp_options.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(RetransmissionTimeoutTest, HasValidInitialRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  EXPECT_EQ(rto_.rto(), options.rto_initial);
}

TEST(RetransmissionTimeoutTest, NegativeValuesDoNotAffectRTO) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(-10));
  EXPECT_GE(rto_.rto(), options.rto_min);
}

TEST(RetransmissionTimeoutTest, TooLargeValuesDoNotAffectRTO) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(options.rto_max + DurationMs(100));
  EXPECT_GE(rto_.rto(), options.rto_min);
}

TEST(RetransmissionTimeoutTest, WillNeverGoBelowMinimumRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(DurationMs(1));
  }
  EXPECT_GE(rto_.rto(), options.rto_min);
}

TEST(RetransmissionTimeoutTest, WillNeverGoAboveMaximumRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(options.rto_max);
    // Adding jitter, which would make it RTO be well above RTT.
    rto_.ObserveRTT(options.rto_max - DurationMs(10));
  }
  EXPECT_LE(rto_.rto(), options.rto_max);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForStableRtt) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 372);
  rto_.ObserveRTT(DurationMs(128));
  EXPECT_THAT(*rto_.rto(), 314);
  rto_.ObserveRTT(DurationMs(123));
  EXPECT_THAT(*rto_.rto(), 268);
  rto_.ObserveRTT(DurationMs(125));
  EXPECT_THAT(*rto_.rto(), 233);
  rto_.ObserveRTT(DurationMs(127));
  EXPECT_THAT(*rto_.rto(), 208);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForUnstableRtt) {
  DcSctpOptions options;
  // This test depends on a specific value of rto_max.
  ASSERT_EQ(options.rto_max, DurationMs(800));

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 372);
  rto_.ObserveRTT(DurationMs(402));
  EXPECT_THAT(*rto_.rto(), 622);
  rto_.ObserveRTT(DurationMs(728));
  EXPECT_THAT(*rto_.rto(), 800);
  rto_.ObserveRTT(DurationMs(89));
  EXPECT_THAT(*rto_.rto(), 800);
  rto_.ObserveRTT(DurationMs(126));
  EXPECT_THAT(*rto_.rto(), 800);
}

TEST(RetransmissionTimeoutTest, WillStabilizeAfterAWhile) {
  DcSctpOptions options;
  // This test depends on a specific value of rto_max.
  ASSERT_EQ(options.rto_max, DurationMs(800));

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(124));
  rto_.ObserveRTT(DurationMs(402));
  rto_.ObserveRTT(DurationMs(728));
  rto_.ObserveRTT(DurationMs(89));
  rto_.ObserveRTT(DurationMs(126));
  EXPECT_THAT(*rto_.rto(), 800);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 800);
  rto_.ObserveRTT(DurationMs(122));
  EXPECT_THAT(*rto_.rto(), 709);
  rto_.ObserveRTT(DurationMs(123));
  EXPECT_THAT(*rto_.rto(), 630);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 561);
  rto_.ObserveRTT(DurationMs(122));
  EXPECT_THAT(*rto_.rto(), 504);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 453);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 409);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 372);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(*rto_.rto(), 339);
}
}  // namespace
}  // namespace dcsctp
