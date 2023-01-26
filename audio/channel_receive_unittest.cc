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

#include "api/crypto/frame_decryptor_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/rtp_rtcp/source/byte_io.h"
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
        audio_device_module_(test::MockAudioDeviceModule::CreateNice()) {}

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

  const std::vector<uint8_t> CreateRtcpSenderReport() {
    std::vector<uint8_t> packet;
    const size_t kRtcpSrLength = 28;  // In bytes.
    packet.resize(kRtcpSrLength);
    packet[0] = 0x80;  // Version 2.
    packet[1] = 0xc8;  // PT = 200, SR.
    // Length in number of 32-bit words - 1.
    ByteWriter<uint16_t>::WriteBigEndian(&packet[2], 6);
    ByteWriter<uint32_t>::WriteBigEndian(&packet[4], kRemoteSsrc);
    ByteWriter<uint64_t>::WriteBigEndian(&packet[8], uint64_t(NtpNow()));
    ByteWriter<uint32_t>::WriteBigEndian(&packet[16], RtpNow());
    ByteWriter<uint32_t>::WriteBigEndian(&packet[20], 0);  // packet count
    ByteWriter<uint32_t>::WriteBigEndian(&packet[24], 0);  // octet count
    // No report blocks.
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

TEST_F(ChannelReceiveTest, CaptureStartTimeBecomesValid) {
  auto channel = CreateTestChannelReceive();
  CallReceiveStatistics stats = channel->GetRTCPStatistics();
  EXPECT_EQ(stats.capture_start_ntp_time_ms_, -1);
  // The first audio frame should not contain info enough to set
  // the capture start NTP time; we have to get RTCP messages first.
  AudioFrame audio_frame_1;
  channel->GetAudioFrameWithInfo(kSampleRateHz, &audio_frame_1);
  stats = channel->GetRTCPStatistics();
  EXPECT_EQ(stats.capture_start_ntp_time_ms_, -1);
  // Receive a couple of sender reports
  auto rtcp_packet_1 = CreateRtcpSenderReport();
  channel->ReceivedRTCPPacket(&rtcp_packet_1[0], rtcp_packet_1.size());
  time_controller_.AdvanceTime(TimeDelta::Seconds(5));

  auto rtcp_packet_2 = CreateRtcpSenderReport();
  channel->ReceivedRTCPPacket(&rtcp_packet_2[0], rtcp_packet_2.size());
  // After this, the audio frame should trigger population of
  // capture_start_ntp_time_ms
  channel->GetAudioFrameWithInfo(kSampleRateHz, &audio_frame_1);
  stats = channel->GetRTCPStatistics();
  EXPECT_NE(stats.capture_start_ntp_time_ms_, -1);
}

}  // namespace voe
}  // namespace webrtc
