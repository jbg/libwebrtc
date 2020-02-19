//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#include "api/voip/audio_channel.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"

namespace webrtc {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Unused;

namespace {

constexpr uint64_t kStartTime = 123456789;
constexpr int16_t kAudioLevel = 3004;  // used for sine wave level
constexpr int kPcmuPayload = 0;
const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};

}  // namespace

class AudioChannelTest : public ::testing::Test {
 public:
  AudioChannelTest()
      : fake_clock_(kStartTime), wave_generator_(1000.0, kAudioLevel) {
    process_thread_ = ProcessThread::Create("ModuleProcessThread");
    audio_mixer_ = AudioMixerImpl::Create();
    task_queue_factory_ = CreateDefaultTaskQueueFactory();
    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
    decoder_factory_ = CreateBuiltinAudioDecoderFactory();
  }

  void SetUp() override {
    audio_channel_ = std::make_unique<AudioChannel>(
        &fake_clock_, task_queue_factory_.get(), process_thread_.get(),
        audio_mixer_.get(), decoder_factory_);
    audio_channel_->GetAudioEgress()->SetEncoder(
        kPcmuPayload, kPcmuFormat,
        encoder_factory_->MakeAudioEncoder(kPcmuPayload, kPcmuFormat,
                                           absl::nullopt));
    audio_channel_->GetAudioIngress()->SetReceiveCodecs(
        {{kPcmuPayload, kPcmuFormat}});
  }

  void TearDown() override { audio_channel_.reset(); }

  std::unique_ptr<AudioFrame> GetAudioFrame(uint32_t timestamp = 0,
                                            bool muted = false) {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = kPcmuFormat.clockrate_hz;
    frame->samples_per_channel_ = kPcmuFormat.clockrate_hz / 100;  // 10 ms.
    frame->num_channels_ = kPcmuFormat.num_channels;
    frame->timestamp_ = timestamp;
    if (muted) {
      frame->Mute();
    } else {
      wave_generator_.GenerateNextFrame(frame.get());
    }
    return frame;
  }

  void InsertPackets(size_t num_packets, bool muted = false) {
    // Two 10 ms audio frames will result in rtp packet with ptime 20.
    size_t required_frame_num = (num_packets * 2);
    for (size_t i = 0; i < required_frame_num; i++) {
      audio_channel_->GetAudioEgress()->SendAudioData(
          GetAudioFrame(kPcmuFormat.clockrate_hz * i, muted));
      // advance 10 ms for each frame
      fake_clock_.AdvanceTimeMilliseconds(10);
    }
  }

  // SimulatedClock doesn't directly affect this testcase as the the
  // AudioFrame's timestamp is driven by GetAudioFrame.
  SimulatedClock fake_clock_;
  SineWaveGenerator wave_generator_;
  std::unique_ptr<AudioChannel> audio_channel_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  rtc::scoped_refptr<AudioMixer> audio_mixer_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  std::unique_ptr<ProcessThread> process_thread_;
};

TEST(DisableLogging, DisableThisTestToSeeWebRTCLog) {
  // By invoking this once here will suppress all the log emitting from webrtc
  // in all following unit tests.
  rtc::LogMessage::ConfigureLogging("none debug");
}

TEST_F(AudioChannelTest, ValidCreationOnAudioIngressEgress) {
  ASSERT_THAT(audio_channel_->GetAudioEgress(), NotNull());
  ASSERT_THAT(audio_channel_->GetAudioIngress(), NotNull());
}

ACTION_P(Notify, n) {
  n->Notify();
}

TEST_F(AudioChannelTest, TransportRegistration) {
  // Use strict mock to detect unwanted callback as failure
  NiceMock<MockTransport> nice_transport;
  audio_channel_->StartSend();
  audio_channel_->RegisterTransport(&nice_transport);

  absl::Notification rtp_done;
  // We expect exactly one rtp and rtcp packet to be transmitted
  EXPECT_CALL(nice_transport, SendRtp(_, _, _))
      .WillOnce(DoAll(Notify(&rtp_done), Return(true)));

  InsertPackets(1);

  rtp_done.WaitForNotificationWithTimeout(absl::Milliseconds(10));
  // rtcp_done.WaitForNotificationWithTimeout(absl::Milliseconds(10));
  ASSERT_TRUE(rtp_done.HasBeenNotified());  // && rtcp_done.HasBeenNotified());
  audio_channel_->DeRegisterTransport();

  // Register and deregister strict mock transport and we shouldn't receive
  // anything now
  absl::Notification no_rtp;
  StrictMock<MockTransport> strict_transport;
  audio_channel_->RegisterTransport(&strict_transport);
  audio_channel_->DeRegisterTransport();
  InsertPackets(1);
  no_rtp.WaitForNotificationWithTimeout(absl::Milliseconds(1));
  ASSERT_FALSE(no_rtp.HasBeenNotified());

  audio_channel_->StopSend();
}

TEST_F(AudioChannelTest, ReceiveRtpPacketAfterStartOnly) {
  // Use strict mock to detect unwanted callback as failure
  NiceMock<MockTransport> nice_transport;
  audio_channel_->RegisterTransport(&nice_transport);

  absl::Notification no_rtp;
  InsertPackets(1);
  no_rtp.WaitForNotificationWithTimeout(absl::Milliseconds(1));
  ASSERT_FALSE(no_rtp.HasBeenNotified());

  absl::Notification rtp_done;
  audio_channel_->StartSend();
  EXPECT_CALL(nice_transport, SendRtp(_, _, _))
      .WillOnce(DoAll(Notify(&rtp_done), Return(true)));
  InsertPackets(1);
  rtp_done.WaitForNotificationWithTimeout(absl::Milliseconds(10));
  ASSERT_TRUE(rtp_done.HasBeenNotified());

  audio_channel_->StopSend();
}

TEST_F(AudioChannelTest, PlayRtpByLocalLoop) {
  NiceMock<MockTransport> nice_transport;
  audio_channel_->RegisterTransport(&nice_transport);
  audio_channel_->StartSend();
  audio_channel_->StartPlay();

  absl::Notification rtp_done;
  auto loop_rtp = [&](const uint8_t* packet, size_t length, Unused) {
    audio_channel_->ReceivedRTPPacket(packet, length);
    rtp_done.Notify();
    return true;
  };
  EXPECT_CALL(nice_transport, SendRtp(_, _, _)).WillOnce(Invoke(loop_rtp));

  InsertPackets(1);

  rtp_done.WaitForNotificationWithTimeout(absl::Milliseconds(10));
  ASSERT_TRUE(rtp_done.HasBeenNotified());

  AudioFrame empty_frame, audio_frame;
  empty_frame.Mute();
  empty_frame.mutable_data();  // this will zero out the data
  audio_frame.CopyFrom(empty_frame);
  audio_mixer_->Mix(/*number_of_channels*/ 1, &audio_frame);

  // we expect now audio frame to pick up something
  EXPECT_NE(memcmp(empty_frame.data(), audio_frame.data(),
                   AudioFrame::kMaxDataSizeBytes),
            0);
  audio_channel_->StopPlay();
  audio_channel_->StopSend();
  audio_channel_->DeRegisterTransport();
}

}  // namespace webrtc
