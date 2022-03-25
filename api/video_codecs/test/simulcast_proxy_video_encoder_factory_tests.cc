/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/mock_video_encoder.h"
#include "api/video_codecs/simulcast_proxy_video_encoder_factory.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Each;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

namespace webrtc {
namespace {
using CodecSupport = VideoEncoderFactory::CodecSupport;
const SdpVideoFormat kFooSdp("Foo");
const SdpVideoFormat kBarLowSdp("Bar", {{"profile", "low"}});
const SdpVideoFormat kBarHighSdp("Bar", {{"profile", "high"}});

struct FooEncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() { return {kFooSdp}; }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return std::make_unique<testing::StrictMock<MockVideoEncoder>>();
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return scalability_mode == "L1T2" || scalability_mode == "L1T3";
  }
};

struct BarEncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {kBarLowSdp, kBarHighSdp};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return std::make_unique<testing::StrictMock<MockVideoEncoder>>();
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return scalability_mode == "L1T2" || scalability_mode == "L1T3" ||
           scalability_mode == "S2T2" || scalability_mode == "S2T3";
  }
};

TEST(SimulcastProxyVideoEncoderFactory, ExtraParameter) {
  SimulcastProxyVideoEncoderFactory<FooEncoderTemplateAdapter> factory;
  SdpVideoFormat modified_format = kFooSdp;
  modified_format.parameters.insert({"extra", "parameter"});
  EXPECT_THAT(factory.CreateVideoEncoder(modified_format), Ne(nullptr));
  EXPECT_THAT(factory.CreateVideoEncoder(kBarLowSdp), Eq(nullptr));
}

}  // namespace
}  // namespace webrtc
