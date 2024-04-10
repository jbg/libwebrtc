/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/transport_layer_feedback.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace rtcp {

using ::testing::SizeIs;

bool PacketInfoEqual(const std::vector<TransportLayerFeedback::PacketInfo>& a,
                     const std::vector<TransportLayerFeedback::PacketInfo>& b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].sequence_number != b[i].sequence_number ||
        a[i].arrival_time_offset.ms() != b[i].arrival_time_offset.ms() ||
        a[i].ecn != b[i].ecn) {
      return false;
    }
  }
  return true;
}

MATCHER_P(MAP_MATCHER, expected_map, "Maps are not equal") {
  if (expected_map.size() != arg.size()) {
    return false;
  }
  return std::all_of(
      expected_map.cbegin(), expected_map.cend(),
      [&](const std::pair<
          uint32_t, std::vector<TransportLayerFeedback::PacketInfo>>& item) {
        auto arg_item = arg.find(item.first);
        if (arg_item == arg.cend()) {
          return false;
        }
        return PacketInfoEqual(arg_item->second, item.second);
      });
}

TEST(TransportLayerFeedbackTest, VerifyBlockLengthNoPackets) {
  TransportLayerFeedback fb({}, /*compact_ntp_timestamp=*/1);
  EXPECT_EQ(fb.BlockLength(),
            /*common header */ 4u /*sender ssrc*/ + 4u + /*timestamp*/ 4u);
}

TEST(TransportLayerFeedbackTest, VerifyBlockLengthTwoSsrcOnePacketEach) {
  std::map<uint32_t /*ssrc*/, std::vector<TransportLayerFeedback::PacketInfo>>
      packets = {{/*ssrc*/ 1,
                  {{.sequence_number = 1,
                    .arrival_time_offset = TimeDelta::Millis(1)}}},
                 {/*ssrc*/ 2,
                  {{.sequence_number = 2,
                    .arrival_time_offset = TimeDelta::Millis(2)}}}};

  TransportLayerFeedback fb(std::move(packets), /*compact_ntp_timestamp=*/1);
  EXPECT_EQ(fb.BlockLength(), /*common header */ 4u + /*sender ssrc*/
                                  4u +
                                  /*timestamp*/ 4u +
                                  /*per ssrc header*/ 2 * 8u +
                                  /* padded packet info per ssrc*/ 2 * 4u);
}

TEST(TransportLayerFeedbackTest, Create) {
  std::map<uint32_t /*ssrc*/, std::vector<TransportLayerFeedback::PacketInfo>>
      packets = {{/*ssrc*/ 1,
                  {{.sequence_number = 1,
                    .arrival_time_offset = TimeDelta::Millis(1)}}},
                 {/*ssrc*/ 2,
                  {{.sequence_number = 2,
                    .arrival_time_offset = TimeDelta::Millis(2)}}}};

  TransportLayerFeedback fb(std::move(packets), /*compact_ntp_timestamp=*/1);

  rtc::Buffer buf(fb.BlockLength());
  size_t position = 0;
  rtc::FunctionView<void(rtc::ArrayView<const uint8_t> packet)> callback;
  EXPECT_TRUE(fb.Create(buf.data(), &position, buf.capacity(), callback));
}

TEST(TransportLayerFeedbackTest, CreateAndParse) {
  const std::map<uint32_t /*ssrc*/,
                 std::vector<TransportLayerFeedback::PacketInfo>>
      kPacketMap = {{/*ssrc*/ 1,
                     {{.sequence_number = 1,
                       .arrival_time_offset = TimeDelta::Millis(2)}}},
                    {/*ssrc*/ 2,
                     {{.sequence_number = 2,
                       .arrival_time_offset = TimeDelta::Millis(1)}}}};

  uint32_t kCompactNtp = 1234;
  TransportLayerFeedback fb(kPacketMap, kCompactNtp);

  rtc::Buffer buffer(fb.BlockLength());
  size_t position = 0;
  rtc::FunctionView<void(rtc::ArrayView<const uint8_t> packet)> callback;
  ASSERT_TRUE(fb.Create(buffer.data(), &position, buffer.capacity(), callback));

  TransportLayerFeedback parsed_fb;

  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
  EXPECT_EQ(header.fmt(), TransportLayerFeedback::kFeedbackMessageType);
  EXPECT_EQ(header.type(), Rtpfb::kPacketType);
  EXPECT_TRUE(parsed_fb.Parse(header));

  EXPECT_EQ(parsed_fb.compact_ntp(), kCompactNtp);

  for (const auto& [ssrc, packets] : parsed_fb.packets()) {
    for (const auto& packet : packets) {
      RTC_LOG(LS_INFO) << "ssrc:" << ssrc << " seq: " << packet.sequence_number
                       << " time_delta: " << packet.arrival_time_offset.ms();
    }
  }

  EXPECT_THAT(parsed_fb.packets(), MAP_MATCHER(kPacketMap));
}

}  // namespace rtcp
}  // namespace webrtc
