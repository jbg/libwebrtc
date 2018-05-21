/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "test/call_test.h"
#include "test/function_video_encoder_factory.h"

namespace webrtc {
namespace {
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kFps = 30;
constexpr int kFramesToObserve = 10;

uint8_t PayloadNameToPayloadType(const std::string& payload_name) {
  if (payload_name == "VP8") {
    return test::CallTest::kPayloadTypeVP8;
  } else if (payload_name == "VP9") {
    return test::CallTest::kPayloadTypeVP9;
  } else if (payload_name == "H264") {
    return test::CallTest::kPayloadTypeH264;
  } else {
    RTC_NOTREACHED();
    return test::CallTest::kVideoSendPayloadType;
  }
}

int RemoveOlderOrEqual(uint32_t timestamp, std::vector<uint32_t>* timestamps) {
  int num_removed = 0;
  while (!timestamps->empty()) {
    auto it = timestamps->begin();
    if (IsNewerTimestamp(*it, timestamp))
      break;

    timestamps->erase(it);
    ++num_removed;
  }
  return num_removed;
}
}  // namespace

class FrameObserver : public test::RtpRtcpObserver,
                      public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FrameObserver() : test::RtpRtcpObserver(test::CallTest::kDefaultTimeoutMs) {}

  void Reset() {
    rtc::CritScope lock(&crit_);
    num_sent_frames_ = 0;
    num_rendered_frames_ = 0;
  }

 private:
  // Sends kFramesToObserve.
  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);

    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, length, &header));
    EXPECT_EQ(header.ssrc, test::CallTest::kVideoSendSsrcs[0]);
    EXPECT_GE(length, header.headerLength + header.paddingLength);
    if ((length - header.headerLength) == header.paddingLength)
      return SEND_PACKET;  // Skip padding, may be sent after OnFrame is called.

    if (!last_timestamp_ || header.timestamp != *last_timestamp_) {
      // New frame.
      if (last_payload_type_) {
        bool new_payload_type = header.payloadType != *last_payload_type_;
        EXPECT_EQ(num_sent_frames_ == 0, new_payload_type)
            << "Payload type should change after reset.";
      }
      // Sent enough frames?
      if (num_sent_frames_ >= kFramesToObserve)
        return DROP_PACKET;

      ++num_sent_frames_;
      sent_timestamps_.push_back(header.timestamp);
    }

    last_timestamp_ = header.timestamp;
    last_payload_type_ = header.payloadType;
    return SEND_PACKET;
  }

  // Verifies that all sent frames are decoded and rendered.
  void OnFrame(const VideoFrame& rendered_frame) override {
    rtc::CritScope lock(&crit_);
    EXPECT_NE(std::find(sent_timestamps_.begin(), sent_timestamps_.end(),
                        rendered_frame.timestamp()),
              sent_timestamps_.end());

    // Remove old timestamps too, only the newest decoded frame is rendered.
    num_rendered_frames_ +=
        RemoveOlderOrEqual(rendered_frame.timestamp(), &sent_timestamps_);

    if (num_rendered_frames_ >= kFramesToObserve) {
      EXPECT_TRUE(sent_timestamps_.empty()) << "All sent frames not decoded.";
      observation_complete_.Set();
    }
  }

  rtc::CriticalSection crit_;
  rtc::Optional<uint32_t> last_timestamp_;
  rtc::Optional<uint8_t> last_payload_type_;
  int num_sent_frames_ RTC_GUARDED_BY(crit_) = 0;
  int num_rendered_frames_ RTC_GUARDED_BY(crit_) = 0;
  std::vector<uint32_t> sent_timestamps_ RTC_GUARDED_BY(crit_);
};

class MultiCodecReceiveTest : public test::CallTest {
 public:
  MultiCodecReceiveTest() {
    task_queue_.SendTask([this]() {
      Call::Config config(event_log_.get());
      CreateCalls(config, config);

      send_transport_.reset(new test::PacketTransport(
          &task_queue_, sender_call_.get(), &observer_,
          test::PacketTransport::kSender, kPayloadTypeMap,
          FakeNetworkPipe::Config()));
      send_transport_->SetReceiver(receiver_call_->Receiver());

      receive_transport_.reset(new test::PacketTransport(
          &task_queue_, receiver_call_.get(), &observer_,
          test::PacketTransport::kReceiver, kPayloadTypeMap,
          FakeNetworkPipe::Config()));
      receive_transport_->SetReceiver(sender_call_->Receiver());
    });
  }

  virtual ~MultiCodecReceiveTest() {
    EXPECT_EQ(nullptr, video_send_stream_);
    EXPECT_TRUE(video_receive_streams_.empty());

    task_queue_.SendTask([this]() {
      send_transport_.reset();
      receive_transport_.reset();
      DestroyCalls();
    });
  }

  void ConfigureDecoders(const std::vector<std::string>& payload_names);
  void ConfigureEncoder(const std::string& payload_name,
                        VideoEncoderFactory* encoder_factory);
  void RunTestWithCodecs(
      const std::vector<std::string>& payload_names,
      const std::vector<VideoEncoderFactory*>& encoder_factories);

 private:
  const std::map<uint8_t, MediaType> kPayloadTypeMap = {
      {CallTest::kPayloadTypeVP8, MediaType::VIDEO},
      {CallTest::kPayloadTypeVP9, MediaType::VIDEO},
      {CallTest::kPayloadTypeH264, MediaType::VIDEO}};
  FrameObserver observer_;
};

void MultiCodecReceiveTest::ConfigureDecoders(
    const std::vector<std::string>& payload_names) {
  // Placing the payload names in a std::set retains the unique names only.
  std::set<std::string> unique_payload_names(payload_names.begin(),
                                             payload_names.end());
  video_receive_configs_[0].decoders.clear();
  for (const auto& payload_name : unique_payload_names) {
    VideoReceiveStream::Decoder decoder = test::CreateMatchingDecoder(
        PayloadNameToPayloadType(payload_name), payload_name);
    allocated_decoders_.push_back(
        std::unique_ptr<VideoDecoder>(decoder.decoder));
    video_receive_configs_[0].decoders.push_back(decoder);
  }
}

void MultiCodecReceiveTest::ConfigureEncoder(
    const std::string& payload_name,
    VideoEncoderFactory* encoder_factory) {
  video_send_config_.encoder_settings.encoder_factory = encoder_factory;
  video_send_config_.rtp.payload_name = payload_name;
  video_send_config_.rtp.payload_type = PayloadNameToPayloadType(payload_name);
  video_encoder_config_.codec_type = PayloadStringToCodecType(payload_name);
}

void MultiCodecReceiveTest::RunTestWithCodecs(
    const std::vector<std::string>& payload_names,
    const std::vector<VideoEncoderFactory*>& encoder_factories) {
  EXPECT_TRUE(!payload_names.empty());
  EXPECT_EQ(payload_names.size(), encoder_factories.size());

  // Create and start call.
  task_queue_.SendTask([this, &encoder_factories, &payload_names]() {
    CreateSendConfig(1, 0, 0, send_transport_.get());
    ConfigureEncoder(payload_names[0], encoder_factories[0]);
    CreateMatchingReceiveConfigs(receive_transport_.get());
    video_receive_configs_[0].renderer = &observer_;
    ConfigureDecoders(payload_names);
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kFps, kWidth, kHeight);
    Start();
  });
  EXPECT_TRUE(observer_.Wait()) << "Timed out waiting for frames.";

  for (size_t i = 1; i < encoder_factories.size(); ++i) {
    // Recreate VideoSendStream with different send codec and resolution.
    task_queue_.SendTask([this, i, &encoder_factories, &payload_names]() {
      frame_generator_capturer_->Stop();
      sender_call_->DestroyVideoSendStream(video_send_stream_);
      observer_.Reset();

      ConfigureEncoder(payload_names[i], encoder_factories[i]);
      video_send_stream_ = sender_call_->CreateVideoSendStream(
          video_send_config_.Copy(), video_encoder_config_.Copy());
      video_send_stream_->Start();
      CreateFrameGeneratorCapturer(kFps, kWidth / 2, kHeight / 2);
      frame_generator_capturer_->Start();
    });
    EXPECT_TRUE(observer_.Wait()) << "Timed out waiting for frames.";
  }

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
  });
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  RunTestWithCodecs({"VP8", "VP9"},
                    {&vp8_encoder_factory, &vp9_encoder_factory});
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9Vp8) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  RunTestWithCodecs(
      {"VP8", "VP9", "VP8"},
      {&vp8_encoder_factory, &vp9_encoder_factory, &vp8_encoder_factory});
}

#if defined(WEBRTC_USE_H264)
TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8H264) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory h264_encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  RunTestWithCodecs({"VP8", "H264"},
                    {&vp8_encoder_factory, &h264_encoder_factory});
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9H264Vp8) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  test::FunctionVideoEncoderFactory h264_encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  RunTestWithCodecs({"VP8", "VP9", "H264", "VP8"},
                    {&vp8_encoder_factory, &vp9_encoder_factory,
                     &h264_encoder_factory, &vp8_encoder_factory});
}
#endif  // defined(WEBRTC_USE_H264)

}  // namespace webrtc
