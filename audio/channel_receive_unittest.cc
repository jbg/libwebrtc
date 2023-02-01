/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive.h"

#include "absl/strings/escaping.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace voe {

const uint32_t kLocalSsrc = 1111;
const uint32_t kRemoteSsrc = 2222;
const int kSampleRateHz = 8000;

class ChannelReceiveTest : public ::testing::Test {
 public:
  ChannelReceiveTest()
      : time_controller_(Timestamp::Seconds(5555)),
        audio_device_module_(test::MockAudioDeviceModule::CreateStrict()) {}

  std::unique_ptr<ChannelReceiveInterface> CreateTestChannelReceive() {
    webrtc::CryptoOptions crypto_options;
    return CreateChannelReceive(
        time_controller_.GetClock(),
        /* neteq_factory= */ nullptr, audio_device_module_.get(), &transport_,
        &event_log_, kLocalSsrc, kRemoteSsrc,
        /* jitter_buffer_max_packets= */ 0,
        /* jitter_buffer_fast_playout= */ false,
        /* jitter_buffer_min_delay_ms= */ 0,
        /* enable_non_sender_rtt= */ false,
        /* decoder_factory= */ nullptr,
        /* codec_pair_id= */ absl::nullopt,
        /* frame_decryptor_interface= */ nullptr, crypto_options,
        /* frame_transformer= */ nullptr);
  }

  NtpTime NtpNow() {
    return NtpTime(rtc::TimeMillis() / 1000, rtc::TimeMillis() % 1000);
  }

  uint32_t RtpNow() {
    // Note - the "random" offset of this timestamp is zero.
    return rtc::TimeMillis() * 1000 / kSampleRateHz;
  }

  RtpPacketReceived CreateRtpPacket() {
    RtpPacketReceived packet;
    packet.set_arrival_time(Timestamp::Millis(rtc::TimeMillis()));
    packet.SetTimestamp(RtpNow());
    packet.SetSsrc(kLocalSsrc);
    packet.SetPayloadType(10);  // L16
    packet.SetPayloadSize(100);
    // TODO(hta): Add some zeroes for the data
    return packet;
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  rtc::scoped_refptr<test::MockAudioDeviceModule> audio_device_module_;
  MockTransport transport_;
  testing::NiceMock<MockRtcEventLog> event_log_;
};

TEST_F(ChannelReceiveTest, CreateAndDestroy) {
  auto channel = CreateTestChannelReceive();
  EXPECT_TRUE(!!channel);
}

TEST_F(ChannelReceiveTest, ReceiveReportGeneratedOnTime) {
  auto channel = CreateTestChannelReceive();
  std::map<int, SdpAudioFormat> codecs;
  SdpAudioFormat l16_codec("L16", 44100, 1);
  codecs.insert({10, l16_codec});
  channel->SetReceiveCodecs(codecs);

  bool rtcp_packet_sent = false;
  EXPECT_CALL(transport_, SendRtcp)
      .WillRepeatedly([&](const uint8_t* packet, size_t length) {
        RTC_LOG(LS_ERROR) << "DEBUG: Time is " << rtc::TimeMillis();
        RTC_LOG(LS_ERROR) << "DEBUG: Sent RTCP packet size " << length;
        RTC_LOG(LS_ERROR) << "DEBUG: Packet content "
                          << rtc::hex_encode_with_delimiter(
                                 absl::string_view(
                                     reinterpret_cast<const char*>(packet),
                                     length),
                                 ' ');
        rtcp_packet_sent = true;
        return true;
      });
  // Send one RTP packet. This causes registration of the SSRC.
  RtpPacketReceived rtp_packet = CreateRtpPacket();
  channel->OnRtpPacket(rtp_packet);
  time_controller_.AdvanceTime(TimeDelta::Millis(2500));

  EXPECT_TRUE(rtcp_packet_sent);
}

}  // namespace voe
}  // namespace webrtc
