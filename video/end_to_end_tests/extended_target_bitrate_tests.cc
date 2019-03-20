/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/rtp_rtcp/source/rtcp_packet/target_bitrate.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {

const int kMinRtcpPacketsToObserve = 3;

enum TestExpectation {
  kNoReport,               // Bitrate report not sent.
  kBitrateNotDistributed,  // Bitrate not distributed across temporal layers.
  kBitrateDistributed      // Bitrate distributed across temporal layers.
};

size_t ExpectedNumTemporalLayers(TestExpectation test_expectation,
                                 size_t num_temporal_layers) {
  switch (test_expectation) {
    case kNoReport:
      return 0u;
    case kBitrateNotDistributed:
      return 1u;
    case kBitrateDistributed:
      return num_temporal_layers;
  }
}
}  // namespace

class ExtendedTargetBitrateTest : public test::CallTest,
                                  public ::testing::WithParamInterface<size_t> {
 public:
  ExtendedTargetBitrateTest() = default;
};

class RtcpObserver : public test::EndToEndTest {
 public:
  RtcpObserver(VideoEncoderFactory* encoder_factory,
               const std::string& payload_name,
               size_t num_ssrcs,
               VideoEncoderConfig::ContentType content_type,
               TestExpectation expectation)
      : EndToEndTest(test::CallTest::kDefaultTimeoutMs),
        encoder_factory_(encoder_factory),
        payload_name_(payload_name),
        num_temporal_layers_(ExtendedTargetBitrateTest::GetParam()),
        expected_num_temporal_layers_(
            ExpectedNumTemporalLayers(expectation, num_temporal_layers_)),
        num_ssrcs_(num_ssrcs),
        expected_num_ssrcs_((expectation == kNoReport) ? 0u : num_ssrcs),
        content_type_(content_type) {}

 private:
  Action OnSendRtcp(const uint8_t* packet, size_t length) override {
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(packet, length));

    observed_rtcp_sr_ += parser.sender_report()->num_packets();

    EXPECT_LE(parser.xr()->num_packets(), 1);
    if (parser.xr()->num_packets() > 0) {
      const uint32_t ssrc = parser.xr()->sender_ssrc();
      if (parser.xr()->target_bitrate() && observed_ssrcs_.count(ssrc) == 0) {
        observed_ssrcs_.insert(ssrc);
        const auto& target_bitrates =
            parser.xr()->target_bitrate()->GetTargetBitrates();
        EXPECT_EQ(expected_num_temporal_layers_, target_bitrates.size());
        for (size_t i = 0; i < target_bitrates.size(); ++i) {
          EXPECT_EQ(0, target_bitrates[i].spatial_layer);
          EXPECT_EQ(i, target_bitrates[i].temporal_layer);
        }
      }
    }

    if (observed_rtcp_sr_ > kMinRtcpPacketsToObserve &&
        observed_ssrcs_.size() == expected_num_ssrcs_) {
      observation_complete_.Set();
    }
    return SEND_PACKET;
  }

  size_t GetNumVideoStreams() const override { return num_ssrcs_; }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    for (size_t i = 0; i < encoder_config->number_of_streams; ++i) {
      encoder_config->simulcast_layers[i].num_temporal_layers =
          num_temporal_layers_;
    }
    encoder_config->content_type = content_type_;
    encoder_config->max_bitrate_bps = 1000000;
    encoder_config->codec_type = PayloadStringToCodecType(payload_name_);
    send_config->encoder_settings.encoder_factory = encoder_factory_;
    send_config->rtp.payload_name = payload_name_;
    send_config->rtp.payload_type =
        ExtendedTargetBitrateTest::kVideoSendPayloadType;
    (*receive_configs)[0].decoders.resize(1);
    (*receive_configs)[0].decoders[0].payload_type =
        send_config->rtp.payload_type;
    (*receive_configs)[0].decoders[0].video_format =
        SdpVideoFormat(send_config->rtp.payload_name);
    (*receive_configs)[0].rtp.rtcp_mode = RtcpMode::kReducedSize;
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait())
        << "Timed out while waiting for RTCP XR packets to be sent.";
  }

  VideoEncoderFactory* const encoder_factory_;
  const std::string payload_name_;
  const size_t num_temporal_layers_;
  const size_t expected_num_temporal_layers_;
  const size_t num_ssrcs_;
  const size_t expected_num_ssrcs_;
  const VideoEncoderConfig::ContentType content_type_;
  int observed_rtcp_sr_ = 0;
  std::set<uint32_t> observed_ssrcs_;
};

INSTANTIATE_TEST_SUITE_P(NumTemporalLayers,
                         ExtendedTargetBitrateTest,
                         ::testing::Values(1, 2));

TEST_P(ExtendedTargetBitrateTest, NoXrSentForVideoWithoutFieldTrial) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  RtcpObserver test(&encoder_factory, "VP8", /*num_ssrcs_*/ 1u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo, kNoReport);
  RunBaseTest(&test);
}

TEST_P(ExtendedTargetBitrateTest, SendsXrVp8) {
  test::ScopedFieldTrials field_trials("WebRTC-Target-Bitrate-Rtcp/Enabled/");
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  RtcpObserver test(&encoder_factory, "VP8", /*num_ssrcs_*/ 1u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo,
                    kBitrateDistributed);
  RunBaseTest(&test);
}

TEST_P(ExtendedTargetBitrateTest, SendsXrVp8Simulcast) {
  test::ScopedFieldTrials field_trials("WebRTC-Target-Bitrate-Rtcp/Enabled/");

  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  RtcpObserver test(&encoder_factory, "VP8", /*num_ssrcs*/ 2u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo,
                    kBitrateDistributed);
  RunBaseTest(&test);
}

TEST_P(ExtendedTargetBitrateTest, SendsXrVp8Screen) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  RtcpObserver test(&encoder_factory, "VP8", /*num_ssrcs*/ 1u,
                    VideoEncoderConfig::ContentType::kScreen,
                    kBitrateDistributed);
  RunBaseTest(&test);
}

TEST_P(ExtendedTargetBitrateTest, SendsXrVp9) {
  test::ScopedFieldTrials field_trials("WebRTC-Target-Bitrate-Rtcp/Enabled/");

  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP9Encoder::Create(); });
  RtcpObserver test(&encoder_factory, "VP9", /*num_ssrcs*/ 1u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo,
                    kBitrateDistributed);
  RunBaseTest(&test);
}

#if defined(WEBRTC_USE_H264)
TEST_P(ExtendedTargetBitrateTest, SendsXrH264) {
  test::ScopedFieldTrials field_trials("WebRTC-Target-Bitrate-Rtcp/Enabled/");

  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  RtcpObserver test(&encoder_factory, "H264", /*num_ssrcs*/ 1u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo,
                    kBitrateDistributed);
  RunBaseTest(&test);
}
#endif  // defined(WEBRTC_USE_H264)

TEST_P(ExtendedTargetBitrateTest, SendsXrVp8SimulcastEncoderAdapter) {
  test::ScopedFieldTrials field_trials("WebRTC-Target-Bitrate-Rtcp/Enabled/");

  InternalEncoderFactory internal_encoder_factory;
  test::FunctionVideoEncoderFactory encoder_factory(
      [&internal_encoder_factory]() {
        return absl::make_unique<SimulcastEncoderAdapter>(
            &internal_encoder_factory, SdpVideoFormat("VP8"));
      });
  RtcpObserver test(&encoder_factory, "VP8", /*num_ssrcs*/ 1u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo,
                    kBitrateDistributed);
  RunBaseTest(&test);
}

TEST_P(ExtendedTargetBitrateTest, SendsXrVp8SimulcastSimulcastEncoderAdapter) {
  test::ScopedFieldTrials field_trials("WebRTC-Target-Bitrate-Rtcp/Enabled/");

  InternalEncoderFactory internal_encoder_factory;
  test::FunctionVideoEncoderFactory encoder_factory(
      [&internal_encoder_factory]() {
        return absl::make_unique<SimulcastEncoderAdapter>(
            &internal_encoder_factory, SdpVideoFormat("VP8"));
      });
  RtcpObserver test(&encoder_factory, "VP8", /*num_ssrcs*/ 2u,
                    VideoEncoderConfig::ContentType::kRealtimeVideo,
                    kBitrateDistributed);
  RunBaseTest(&test);
}

}  // namespace webrtc
