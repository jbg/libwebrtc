/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtcp_packet/remote_estimate.h"

#include "test/gtest.h"

namespace webrtc {
namespace rtcp {
TEST(RemoteEstimateTest, EncodesCapacityBounds) {
  NetworkStateEstimate src;
  src.link_capacity_lower = DataRate::kbps(100);
  src.link_capacity_upper = DataRate::kbps(1000000);
  rtc::Buffer data = GetRemoteEstimateSerializer()->Serialize(src);

  NetworkStateEstimate dst;
  EXPECT_TRUE(GetRemoteEstimateSerializer()->Parse(data, &dst));
  EXPECT_EQ(src.link_capacity_lower, dst.link_capacity_lower);
  EXPECT_EQ(src.link_capacity_upper, dst.link_capacity_upper);
}

TEST(RemoteEstimateTest, EncodesInfinite) {
  NetworkStateEstimate src;
  // White box testing: We know that the value is stored in an unsigned 24 int
  // with kbps resolution. We expected it be represented as plus infinity.
  src.link_capacity_lower = DataRate::kbps(2 << 24);
  src.link_capacity_upper = DataRate::PlusInfinity();
  rtc::Buffer data = GetRemoteEstimateSerializer()->Serialize(src);

  NetworkStateEstimate dst;
  EXPECT_TRUE(GetRemoteEstimateSerializer()->Parse(data, &dst));
  EXPECT_TRUE(dst.link_capacity_lower.IsPlusInfinity());
  EXPECT_TRUE(dst.link_capacity_upper.IsPlusInfinity());
}

TEST(RemoteEstimateTest, CapsNegativeAtZero) {
  NetworkStateEstimate src;
  // We should not try to store minus infinity, as that's invalid. But if we do,
  // we expect it to be capped to zero for now.
  src.link_capacity_lower = DataRate::MinusInfinity();
  rtc::Buffer data = GetRemoteEstimateSerializer()->Serialize(src);
  NetworkStateEstimate dst;
  EXPECT_TRUE(GetRemoteEstimateSerializer()->Parse(data, &dst));
  EXPECT_TRUE(dst.link_capacity_lower.IsZero());
}
}  // namespace rtcp
}  // namespace webrtc
