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

using ::testing::ElementsAreArray;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Not;

}  // namespace

TEST(RtpPacketInfoTest, Ssrc) {
  const uint32_t value = 4038189233;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs.set_ssrc(value);
  EXPECT_THAT(rhs.ssrc(), value);

  EXPECT_THAT(lhs == rhs, IsFalse());
  EXPECT_THAT(lhs != rhs, IsTrue());

  lhs = rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs = RtpPacketInfo();
  EXPECT_THAT(rhs.ssrc(), Not(value));

  rhs = RtpPacketInfo(value, {}, {}, {}, {}, {});
  EXPECT_THAT(rhs.ssrc(), value);
}

TEST(RtpPacketInfoTest, Csrcs) {
  const std::vector<uint32_t> value = {4038189233, 3016333617, 1207992985};

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs.set_csrcs(value);
  EXPECT_THAT(rhs.csrcs(), value);

  EXPECT_THAT(lhs == rhs, IsFalse());
  EXPECT_THAT(lhs != rhs, IsTrue());

  lhs = rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs = RtpPacketInfo();
  EXPECT_THAT(rhs.csrcs(), Not(value));

  rhs = RtpPacketInfo({}, value, {}, {}, {}, {});
  EXPECT_THAT(rhs.csrcs(), value);
}

TEST(RtpPacketInfoTest, SequenceNumber) {
  const uint16_t value = 20238;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs.set_sequence_number(value);
  EXPECT_THAT(rhs.sequence_number(), value);

  EXPECT_THAT(lhs == rhs, IsFalse());
  EXPECT_THAT(lhs != rhs, IsTrue());

  lhs = rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs = RtpPacketInfo();
  EXPECT_THAT(rhs.sequence_number(), Not(value));

  rhs = RtpPacketInfo({}, {}, value, {}, {}, {});
  EXPECT_THAT(rhs.sequence_number(), value);
}

TEST(RtpPacketInfoTest, RtpTimestamp) {
  const uint32_t value = 4038189233;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs.set_rtp_timestamp(value);
  EXPECT_THAT(rhs.rtp_timestamp(), value);

  EXPECT_THAT(lhs == rhs, IsFalse());
  EXPECT_THAT(lhs != rhs, IsTrue());

  lhs = rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs = RtpPacketInfo();
  EXPECT_THAT(rhs.rtp_timestamp(), Not(value));

  rhs = RtpPacketInfo({}, {}, {}, value, {}, {});
  EXPECT_THAT(rhs.rtp_timestamp(), value);
}

TEST(RtpPacketInfoTest, AudioLevel) {
  const absl::optional<uint8_t> value = 31;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs.set_audio_level(value);
  EXPECT_THAT(rhs.audio_level(), value);

  EXPECT_THAT(lhs == rhs, IsFalse());
  EXPECT_THAT(lhs != rhs, IsTrue());

  lhs = rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs = RtpPacketInfo();
  EXPECT_THAT(rhs.audio_level(), Not(value));

  rhs = RtpPacketInfo({}, {}, {}, {}, value, {});
  EXPECT_THAT(rhs.audio_level(), value);
}

TEST(RtpPacketInfoTest, ReceiveTimeMs) {
  const int64_t value = 8868963877546349045LL;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs.set_receive_time_ms(value);
  EXPECT_THAT(rhs.receive_time_ms(), value);

  EXPECT_THAT(lhs == rhs, IsFalse());
  EXPECT_THAT(lhs != rhs, IsTrue());

  lhs = rhs;

  EXPECT_THAT(lhs == rhs, IsTrue());
  EXPECT_THAT(lhs != rhs, IsFalse());

  rhs = RtpPacketInfo();
  EXPECT_THAT(rhs.receive_time_ms(), Not(value));

  rhs = RtpPacketInfo({}, {}, {}, {}, {}, value);
  EXPECT_THAT(rhs.receive_time_ms(), value);
}

}  // namespace webrtc
