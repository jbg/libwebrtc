/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_utils.h"

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/priority.h"
#include "pc/sctp_data_channel_constants.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "test/gtest.h"

class SctpUtilsTest : public ::testing::Test {
 public:
  void VerifyOpenMessageFormat(const rtc::CopyOnWriteBuffer& packet,
                               const std::string& label,
                               const webrtc::DataChannelInit& config) {
    uint8_t message_type;
    uint8_t channel_type;
    uint32_t reliability;
    uint16_t priority;
    uint16_t label_length;
    uint16_t protocol_length;

    rtc::ByteBufferReader buffer(packet.data<char>(), packet.size());
    ASSERT_TRUE(buffer.ReadUInt8(&message_type));
    EXPECT_EQ(0x03, message_type);

    ASSERT_TRUE(buffer.ReadUInt8(&channel_type));
    if (config.ordered) {
      EXPECT_EQ(
          config.maxRetransmits ? 0x01 : (config.maxRetransmitTime ? 0x02 : 0),
          channel_type);
    } else {
      EXPECT_EQ(config.maxRetransmits
                    ? 0x81
                    : (config.maxRetransmitTime ? 0x82 : 0x80),
                channel_type);
    }

    ASSERT_TRUE(buffer.ReadUInt16(&priority));
    if (config.priority) {
      // Exact values are checked by round-trip conversion, but
      // all values defined are greater than zero.
      EXPECT_GT(priority, 0);
    } else {
      EXPECT_EQ(priority, 0);
    }

    ASSERT_TRUE(buffer.ReadUInt32(&reliability));
    if (config.maxRetransmits || config.maxRetransmitTime) {
      EXPECT_EQ(config.maxRetransmits ? *config.maxRetransmits
                                      : *config.maxRetransmitTime,
                static_cast<int>(reliability));
    }

    ASSERT_TRUE(buffer.ReadUInt16(&label_length));
    ASSERT_TRUE(buffer.ReadUInt16(&protocol_length));
    EXPECT_EQ(label.size(), label_length);
    EXPECT_EQ(config.protocol.size(), protocol_length);

    std::string label_output;
    ASSERT_TRUE(buffer.ReadString(&label_output, label_length));
    EXPECT_EQ(label, label_output);
    std::string protocol_output;
    ASSERT_TRUE(buffer.ReadString(&protocol_output, protocol_length));
    EXPECT_EQ(config.protocol, protocol_output);
  }
};

TEST_F(SctpUtilsTest, WriteParseOpenMessageWithOrderedReliable) {
  webrtc::DataChannelInit config;
  std::string label = "abc";
  config.protocol = "y";

  rtc::CopyOnWriteBuffer packet;
  ASSERT_TRUE(webrtc::WriteDataChannelOpenMessage(label, config, &packet));

  VerifyOpenMessageFormat(packet, label, config);

  absl::optional<webrtc::DataChannelOpenMessage> message =
      webrtc::ParseDataChannelOpenMessage(packet);
  ASSERT_TRUE(message);

  EXPECT_EQ(label, message->label);
  EXPECT_EQ(config.protocol, message->configuration.protocol);
  EXPECT_EQ(config.ordered, message->configuration.ordered);
  EXPECT_EQ(config.maxRetransmitTime, message->configuration.maxRetransmitTime);
  EXPECT_EQ(config.maxRetransmits, message->configuration.maxRetransmits);
}

TEST_F(SctpUtilsTest, WriteParseOpenMessageWithMaxRetransmitTime) {
  webrtc::DataChannelInit config;
  std::string label = "abc";
  config.ordered = false;
  config.maxRetransmitTime = 10;
  config.protocol = "y";

  rtc::CopyOnWriteBuffer packet;
  ASSERT_TRUE(webrtc::WriteDataChannelOpenMessage(label, config, &packet));

  VerifyOpenMessageFormat(packet, label, config);

  absl::optional<webrtc::DataChannelOpenMessage> message =
      webrtc::ParseDataChannelOpenMessage(packet);
  ASSERT_TRUE(message);

  EXPECT_EQ(label, message->label);
  EXPECT_EQ(config.protocol, message->configuration.protocol);
  EXPECT_EQ(config.ordered, message->configuration.ordered);
  EXPECT_EQ(config.maxRetransmitTime, message->configuration.maxRetransmitTime);
  EXPECT_EQ(config.maxRetransmits, message->configuration.maxRetransmits);
}

TEST_F(SctpUtilsTest, WriteParseOpenMessageWithMaxRetransmits) {
  webrtc::DataChannelInit config;
  std::string label = "abc";
  config.maxRetransmits = 10;
  config.protocol = "y";

  rtc::CopyOnWriteBuffer packet;
  ASSERT_TRUE(webrtc::WriteDataChannelOpenMessage(label, config, &packet));

  VerifyOpenMessageFormat(packet, label, config);

  absl::optional<webrtc::DataChannelOpenMessage> message =
      webrtc::ParseDataChannelOpenMessage(packet);
  ASSERT_TRUE(message);

  EXPECT_EQ(label, message->label);
  EXPECT_EQ(config.protocol, message->configuration.protocol);
  EXPECT_EQ(config.ordered, message->configuration.ordered);
  EXPECT_EQ(config.maxRetransmitTime, message->configuration.maxRetransmitTime);
  EXPECT_EQ(config.maxRetransmits, message->configuration.maxRetransmits);
}

TEST_F(SctpUtilsTest, WriteParseOpenMessageWithPriority) {
  webrtc::DataChannelInit config;
  std::string label = "abc";
  config.protocol = "y";
  config.priority = webrtc::Priority::kVeryLow;

  rtc::CopyOnWriteBuffer packet;
  ASSERT_TRUE(webrtc::WriteDataChannelOpenMessage(label, config, &packet));

  VerifyOpenMessageFormat(packet, label, config);

  absl::optional<webrtc::DataChannelOpenMessage> message =
      webrtc::ParseDataChannelOpenMessage(packet);
  ASSERT_TRUE(message);

  EXPECT_EQ(label, message->label);
  ASSERT_TRUE(message->configuration.priority);
  EXPECT_EQ(*config.priority, *message->configuration.priority);
  EXPECT_EQ(webrtc::DataChannelPriority(webrtc::kDataChannelPriorityVeryLow),
            message->configuration.internal_priority);
}

TEST_F(SctpUtilsTest, WriteParseAckMessage) {
  rtc::CopyOnWriteBuffer packet;
  webrtc::WriteDataChannelOpenAckMessage(&packet);

  uint8_t message_type;
  rtc::ByteBufferReader buffer(packet.data<char>(), packet.size());
  ASSERT_TRUE(buffer.ReadUInt8(&message_type));
  EXPECT_EQ(0x02, message_type);

  EXPECT_TRUE(webrtc::ParseDataChannelOpenAckMessage(packet));
}

TEST_F(SctpUtilsTest, TestIsOpenMessage) {
  rtc::CopyOnWriteBuffer open(1);
  open.MutableData()[0] = 0x03;
  EXPECT_TRUE(webrtc::IsOpenMessage(open));

  rtc::CopyOnWriteBuffer openAck(1);
  openAck.MutableData()[0] = 0x02;
  EXPECT_FALSE(webrtc::IsOpenMessage(openAck));

  rtc::CopyOnWriteBuffer invalid(1);
  invalid.MutableData()[0] = 0x01;
  EXPECT_FALSE(webrtc::IsOpenMessage(invalid));

  rtc::CopyOnWriteBuffer empty;
  EXPECT_FALSE(webrtc::IsOpenMessage(empty));
}
