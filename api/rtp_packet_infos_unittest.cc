/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_packet_infos.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsFalse;

template <typename Iterator>
RtpPacketInfos::vector_type ToVector(Iterator begin, Iterator end) {
  return RtpPacketInfos::vector_type(begin, end);
}

}  // namespace

TEST(RtpPacketInfosTest, BasicFunctionality) {
  RtpPacketInfo p0(123, {1, 2}, 42, 89, 5, 7);
  RtpPacketInfo p1(456, {3, 4}, 37, 89, 4, 1);
  RtpPacketInfo p2(789, {5, 6}, 91, 88, 1, 7);

  RtpPacketInfos x({p0, p1, p2});

  ASSERT_THAT(x.size(), 3);

  EXPECT_THAT(x[0], p0);
  EXPECT_THAT(x[1], p1);
  EXPECT_THAT(x[2], p2);

  EXPECT_THAT(x.front(), p0);
  EXPECT_THAT(x.back(), p2);

  EXPECT_THAT(ToVector(x.begin(), x.end()), ElementsAre(p0, p1, p2));
  EXPECT_THAT(ToVector(x.rbegin(), x.rend()), ElementsAre(p2, p1, p0));

  EXPECT_THAT(ToVector(x.cbegin(), x.cend()), ElementsAre(p0, p1, p2));
  EXPECT_THAT(ToVector(x.crbegin(), x.crend()), ElementsAre(p2, p1, p0));

  EXPECT_THAT(x.empty(), IsFalse());
}

TEST(RtpPacketInfosTest, CopyShareData) {
  RtpPacketInfo p0(123, {1, 2}, 42, 89, 5, 7);
  RtpPacketInfo p1(456, {3, 4}, 37, 89, 4, 1);
  RtpPacketInfo p2(789, {5, 6}, 91, 88, 1, 7);

  RtpPacketInfos lhs({p0, p1, p2});
  RtpPacketInfos rhs = lhs;

  ASSERT_THAT(lhs.size(), 3);
  ASSERT_THAT(rhs.size(), 3);

  for (size_t i = 0; i < lhs.size(); ++i) {
    EXPECT_THAT(lhs[i], rhs[i]);
  }

  EXPECT_THAT(lhs.front(), rhs.front());
  EXPECT_THAT(lhs.back(), rhs.back());

  EXPECT_THAT(lhs.begin(), rhs.begin());
  EXPECT_THAT(lhs.end(), rhs.end());
  EXPECT_THAT(lhs.rbegin(), rhs.rbegin());
  EXPECT_THAT(lhs.rend(), rhs.rend());

  EXPECT_THAT(lhs.cbegin(), rhs.cbegin());
  EXPECT_THAT(lhs.cend(), rhs.cend());
  EXPECT_THAT(lhs.crbegin(), rhs.crbegin());
  EXPECT_THAT(lhs.crend(), rhs.crend());

  EXPECT_THAT(lhs.empty(), rhs.empty());
}

}  // namespace webrtc
