/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_sender_audio.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const int kAudioLevelExtensionId = 9;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint8_t kAudioLevel = 0x5a;
const uint64_t kStartTime = 123456789;

using ::testing::_;
using ::testing::ElementsAreArray;

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest() : total_bytes_sent_(0) {
    receivers_extensions_.Register(kRtpExtensionAudioLevel,
                                   kAudioLevelExtensionId);
  }

  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    last_options_ = options;
    total_bytes_sent_ += len;
    sent_packets_.push_back(RtpPacketReceived(&receivers_extensions_));
    EXPECT_TRUE(sent_packets_.back().Parse(data, len));
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override { return false; }
  const RtpPacketReceived& last_sent_packet() { return sent_packets_.back(); }
  int packets_sent() { return sent_packets_.size(); }

  size_t total_bytes_sent_;
  PacketOptions last_options_;
  std::vector<RtpPacketReceived> sent_packets_;

 private:
  RtpHeaderExtensionMap receivers_extensions_;
};

}  // namespace

class RtpSenderAudioTest : public ::testing::Test {
 public:
  RtpSenderAudioTest()
      : fake_clock_(kStartTime),
        retransmission_rate_limiter_(&fake_clock_, 1000) {}

  void SetUp() override {
    rtp_sender_.reset(new RTPSender(
        true, &fake_clock_, &transport_, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, &retransmission_rate_limiter_,
        nullptr, false, nullptr, false, false));
    rtp_sender_->SetSSRC(kSsrc);
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }

  SimulatedClock fake_clock_;
  RateLimiter retransmission_rate_limiter_;
  LoopbackTransportTest transport_;
  // TODO(nisse): No pointer
  std::unique_ptr<RTPSender> rtp_sender_;
};

TEST_F(RtpSenderAudioTest, SendAudio) {
  const char payload_name[] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  RTPSenderAudio rtp_sender_audio(&fake_clock_, rtp_sender_.get());
  ASSERT_EQ(0, rtp_sender_audio.RegisterAudioPayload(payload_name, payload_type,
                                                     48000, 0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_TRUE(rtp_sender_audio.SendAudio(kAudioFrameCN, payload_type, 4321,
                                         payload, sizeof(payload)));

  auto sent_payload = transport_.last_sent_packet().payload();
  EXPECT_THAT(sent_payload, ElementsAreArray(payload));
}

TEST_F(RtpSenderAudioTest, SendAudioWithAudioLevelExtension) {
  RTPSenderAudio rtp_sender_audio(&fake_clock_, rtp_sender_.get());
  EXPECT_EQ(0, rtp_sender_audio.SetAudioLevel(kAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));

  const char payload_name[] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_audio.RegisterAudioPayload(payload_name, payload_type,
                                                     48000, 0, 1500));

  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_TRUE(rtp_sender_audio.SendAudio(kAudioFrameCN, payload_type, 4321,
                                         payload, sizeof(payload)));

  auto sent_payload = transport_.last_sent_packet().payload();
  EXPECT_THAT(sent_payload, ElementsAreArray(payload));
  // Verify AudioLevel extension.
  bool voice_activity;
  uint8_t audio_level;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<AudioLevel>(
      &voice_activity, &audio_level));
  EXPECT_EQ(kAudioLevel, audio_level);
  EXPECT_FALSE(voice_activity);
}

// As RFC4733, named telephone events are carried as part of the audio stream
// and must use the same sequence number and timestamp base as the regular
// audio channel.
// This test checks the marker bit for the first packet and the consequent
// packets of the same telephone event. Since it is specifically for DTMF
// events, ignoring audio packets and sending kEmptyFrame instead of those.
TEST_F(RtpSenderAudioTest, CheckMarkerBitForTelephoneEvents) {
  const char* kDtmfPayloadName = "telephone-event";
  const uint32_t kPayloadFrequency = 8000;
  const uint8_t kPayloadType = 126;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kDtmfPayloadName, kPayloadType,
                                            kPayloadFrequency, 0, 0));
  // For Telephone events, payload is not added to the registered payload list,
  // it will register only the payload used for audio stream.
  // Registering the payload again for audio stream with different payload name.
  const char* kPayloadName = "payload_name";
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kPayloadName, kPayloadType,
                                            kPayloadFrequency, 1, 0));
  // Start time is arbitrary.
  uint32_t capture_timestamp = fake_clock_.TimeInMilliseconds();
  // DTMF event key=9, duration=500 and attenuationdB=10
  rtp_sender_->SendTelephoneEvent(9, 500, 10);
  // During start, it takes the starting timestamp as last sent timestamp.
  // The duration is calculated as the difference of current and last sent
  // timestamp. So for first call it will skip since the duration is zero.
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(kEmptyFrame, kPayloadType,
                                            capture_timestamp, 0, nullptr, 0,
                                            nullptr, nullptr, nullptr, 0));
  // DTMF Sample Length is (Frequency/1000) * Duration.
  // So in this case, it is (8000/1000) * 500 = 4000.
  // Sending it as two packets.
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kEmptyFrame, kPayloadType, capture_timestamp + 2000, 0, nullptr, 0,
      nullptr, nullptr, nullptr, 0));

  // Marker Bit should be set to 1 for first packet.
  EXPECT_TRUE(transport_.last_sent_packet().Marker());

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kEmptyFrame, kPayloadType, capture_timestamp + 4000, 0, nullptr, 0,
      nullptr, nullptr, nullptr, 0));
  // Marker Bit should be set to 0 for rest of the packets.
  EXPECT_FALSE(transport_.last_sent_packet().Marker());
}

}  // namespace webrtc
