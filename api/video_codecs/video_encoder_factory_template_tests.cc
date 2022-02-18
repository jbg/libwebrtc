/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_adapters.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::SizeIs;

namespace webrtc {

namespace {

struct FooEncoder {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {SdpVideoFormat("FooCodec")};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return nullptr;
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return scalability_mode == "L1T1" || scalability_mode == "L1T2" ||
           scalability_mode == "L1T3";
  }
};

struct BarEncoder {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {SdpVideoFormat("BarCodec", {{"profile", "low"}}),
            SdpVideoFormat("BarCodec", {{"profile", "high"}})};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return nullptr;
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return true;
  }
};

TEST(VideoEncoderFactoryType, LibvpxVp8) {
  auto factory = CreateVideoEncoderFactory<LibvpxVp8EncoderTemplateAdapter>();
  EXPECT_THAT(factory->GetSupportedFormats(),
              ElementsAre(SdpVideoFormat("VP8")));
}

}  // namespace
}  // namespace webrtc
