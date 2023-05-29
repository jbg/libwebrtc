/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_audio.h"

#include <memory>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_packet_sender.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {

namespace {
enum : int {  // The first valid value is 1.
  kAudioLevelExtensionId = 1,
  kAbsoluteCaptureTimeExtensionId = 2,
};

const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint8_t kAudioLevel = 0x5a;
const uint64_t kStartTime = 123456789;

using ::testing::AnyNumber;
using ::testing::ElementsAreArray;
using ::testing::Property;
using ::testing::SizeIs;

}  // namespace

class RtpSenderAudioTest : public ::testing::Test {
 public:
  RtpSenderAudioTest()
      : fake_clock_(kStartTime),
        rtp_module_(ModuleRtpRtcpImpl2::Create([&] {
          RtpRtcpInterface::Configuration config;
          config.audio = true;
          config.clock = &fake_clock_;
          config.outgoing_transport = &transport_;
          config.local_media_ssrc = kSsrc;
          config.paced_sender = &mock_paced_sender_;
          return config;
        }())),
        rtp_sender_audio_(
            std::make_unique<RTPSenderAudio>(&fake_clock_,
                                             rtp_module_->RtpSender())) {
    rtp_module_->SetSequenceNumber(kSeqNum);
  }

  rtc::AutoThread main_thread_;
  SimulatedClock fake_clock_;
  MockTransport transport_;
  MockRtpPacketSender mock_paced_sender_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_module_;
  std::unique_ptr<RTPSenderAudio> rtp_sender_audio_;
};

TEST_F(RtpSenderAudioTest, PaceAudio) {
  const char payload_name[] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_audio_->RegisterAudioPayload(
                   payload_name, payload_type, 48000, 0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(Property(
                  &RtpPacketToSend::payload, ElementsAreArray(payload))))));
  ASSERT_TRUE(
      rtp_sender_audio_->SendAudio(AudioFrameType::kAudioFrameCN, payload_type,
                                   4321, payload, sizeof(payload),
                                   /*absolute_capture_timestamp_ms=*/0));
}

TEST_F(RtpSenderAudioTest, PaceWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_audio_->SetAudioLevel(kAudioLevel));
  rtp_module_->RegisterRtpHeaderExtension(AudioLevel::Uri(),
                                          kAudioLevelExtensionId);

  const char payload_name[] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_audio_->RegisterAudioPayload(
                   payload_name, payload_type, 48000, 0, 1500));

  uint8_t payload[] = {47, 11, 32, 93, 89};

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        ASSERT_THAT(packets, SizeIs(1));
        // Verify AudioLevel extension.
        bool voice_activity;
        uint8_t audio_level;
        EXPECT_TRUE(packets[0]->GetExtension<AudioLevel>(&voice_activity,
                                                         &audio_level));
        EXPECT_EQ(kAudioLevel, audio_level);
        EXPECT_FALSE(voice_activity);
      });
  ASSERT_TRUE(
      rtp_sender_audio_->SendAudio(AudioFrameType::kAudioFrameCN, payload_type,
                                   4321, payload, sizeof(payload),
                                   /*absolute_capture_timestamp_ms=*/0));
}

TEST_F(RtpSenderAudioTest, PaceAudioWithoutAbsoluteCaptureTime) {
  constexpr uint32_t kAbsoluteCaptureTimestampMs = 521;
  const char payload_name[] = "audio";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_audio_->RegisterAudioPayload(
                   payload_name, payload_type, 48000, 0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        ASSERT_THAT(packets, SizeIs(1));
        EXPECT_FALSE(packets[0]->HasExtension<AbsoluteCaptureTimeExtension>());
      });
  ASSERT_TRUE(rtp_sender_audio_->SendAudio(
      AudioFrameType::kAudioFrameCN, payload_type, 4321, payload,
      sizeof(payload), kAbsoluteCaptureTimestampMs));
}

TEST_F(RtpSenderAudioTest,
       SendAudioWithAbsoluteCaptureTimeWithCaptureClockOffset) {
  rtp_module_->RegisterRtpHeaderExtension(AbsoluteCaptureTimeExtension::Uri(),
                                          kAbsoluteCaptureTimeExtensionId);
  constexpr uint32_t kAbsoluteCaptureTimestampMs = 521;
  const char payload_name[] = "audio";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_audio_->RegisterAudioPayload(
                   payload_name, payload_type, 48000, 0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        ASSERT_THAT(packets, SizeIs(1));
        auto absolute_capture_time =
            packets[0]->GetExtension<AbsoluteCaptureTimeExtension>();
        ASSERT_TRUE(absolute_capture_time);
        EXPECT_EQ(absolute_capture_time->absolute_capture_timestamp,
                  Int64MsToUQ32x32(
                      fake_clock_.ConvertTimestampToNtpTimeInMilliseconds(
                          kAbsoluteCaptureTimestampMs)));
        EXPECT_EQ(absolute_capture_time->estimated_capture_clock_offset, 0);
      });
  ASSERT_TRUE(rtp_sender_audio_->SendAudio(
      AudioFrameType::kAudioFrameCN, payload_type, 4321, payload,
      sizeof(payload), kAbsoluteCaptureTimestampMs));
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
  ASSERT_EQ(0, rtp_sender_audio_->RegisterAudioPayload(
                   kDtmfPayloadName, kPayloadType, kPayloadFrequency, 0, 0));
  // For Telephone events, payload is not added to the registered payload list,
  // it will register only the payload used for audio stream.
  // Registering the payload again for audio stream with different payload name.
  const char* kPayloadName = "payload_name";
  ASSERT_EQ(0, rtp_sender_audio_->RegisterAudioPayload(
                   kPayloadName, kPayloadType, kPayloadFrequency, 1, 0));
  // Start time is arbitrary.
  uint32_t capture_timestamp = fake_clock_.TimeInMilliseconds();
  // DTMF event key=9, duration=500 and attenuationdB=10
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets).Times(0);
  rtp_sender_audio_->SendTelephoneEvent(9, 500, 10);
  // During start, it takes the starting timestamp as last sent timestamp.
  // The duration is calculated as the difference of current and last sent
  // timestamp. So for first call it will skip since the duration is zero.
  ASSERT_TRUE(rtp_sender_audio_->SendAudio(
      AudioFrameType::kEmptyFrame, kPayloadType, capture_timestamp, nullptr, 0,
      /*absolute_capture_time_ms=0*/ 0));

  // DTMF Sample Length is (Frequency/1000) * Duration.
  // So in this case, it is (8000/1000) * 500 = 4000.
  // Sending it as two packets.
  // Marker Bit should be set to 1 for first packet.
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(ElementsAre(Pointee(Property(
                                      &RtpPacketToSend::Marker, true)))));
  ASSERT_TRUE(rtp_sender_audio_->SendAudio(AudioFrameType::kEmptyFrame,
                                           kPayloadType,
                                           capture_timestamp + 2000, nullptr, 0,
                                           /*absolute_capture_time_ms=0*/ 0));

  // Marker Bit should be set to 0 for rest of the packets.
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(ElementsAre(Pointee(Property(
                                      &RtpPacketToSend::Marker, false)))))
      .Times(AnyNumber());
  ASSERT_TRUE(rtp_sender_audio_->SendAudio(AudioFrameType::kEmptyFrame,
                                           kPayloadType,
                                           capture_timestamp + 4000, nullptr, 0,
                                           /*absolute_capture_time_ms=0*/ 0));
}

}  // namespace webrtc
