/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/fec_private_tables_bursty.h"
#include "modules/rtp_rtcp/source/fec_private_tables_random.h"

#include "test/gtest.h"

namespace webrtc {
namespace fec_private_tables {

TEST(BurstyTable, TestLookup) {
  rtc::ArrayView<const uint8_t> result = LookUpInBurstyTable(0, 0, 0);
  // Should match kMaskBursty1_1.
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0x80u, result[0]);

  result = LookUpInBurstyTable(3, 0, 0);
  // Should match kMaskBursty4_1.
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0xf0u, result[0]);
  EXPECT_EQ(0x00u, result[1]);

  result = LookUpInBurstyTable(1, 1, 0);
  // Should match kMaskBursty2_2.
  EXPECT_EQ(4u, result.size());
  EXPECT_EQ(0x80u, result[0]);
  EXPECT_EQ(0xc0u, result[2]);

  result = LookUpInBurstyTable(11, 11, 0);
  // Should match kMaskBursty12_12.
  EXPECT_EQ(24u, result.size());
  EXPECT_EQ(0x80u, result[0]);
  EXPECT_EQ(0x30u, result[23]);
}

TEST(RandomTable, TestLookup) {
  rtc::ArrayView<const uint8_t> result;
  result = LookUpInRandomTable(0, 0, 0);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0x80u, result[0]);
  EXPECT_EQ(0x00u, result[1]);

  result = LookUpInRandomTable(4, 1, 0);
  // kMaskRandom5_2.
  EXPECT_EQ(4u, result.size());
  EXPECT_EQ(0xa8u, result[0]);
  EXPECT_EQ(0xd0u, result[2]);

  result = LookUpInRandomTable(16, 0, 0);
  // kMaskRandom17_1.
  EXPECT_EQ(6u, result.size());
  EXPECT_EQ(0xffu, result[0]);
  EXPECT_EQ(0x00u, result[5]);

  result = LookUpInRandomTable(47, 47, 0);
  // kMaskRandom48_48.
  EXPECT_EQ(6u * 48, result.size());
  EXPECT_EQ(0x10u, result[0]);
  EXPECT_EQ(0x02u, result[6]);
}

}  // namespace fec_private_tables
}  // namespace webrtc
