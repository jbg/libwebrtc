/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/test/create_videocodec_test_fixture.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/media_constants.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {
// Codec settings.
constexpr int kCifWidth = 352;
constexpr int kCifHeight = 288;
constexpr int kHdWidth = 1280;
constexpr int kHdHeight = 720;
constexpr int kNumFramesLong = 300;

VideoCodecTestFixture::Config CreateCifConfig() {
  VideoCodecTestFixture::Config config;
  config.filename = "foreman_cif";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFramesLong;
  config.use_single_core = true;
  return config;
}

VideoCodecTestFixture::Config CreateHdConfig() {
  VideoCodecTestFixture::Config config;
  config.filename = "ConferenceMotion_1280_720_50";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFramesLong;
  config.use_single_core = true;
  return config;
}

TEST(VideoCodecTestLibaom, HighBitrateAV1) {
  auto config = CreateCifConfig();
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kCifWidth, kCifHeight);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 34, 0.94, 0.92}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestLibaom, VeryLowBitrateAV1) {
  auto config = CreateCifConfig();
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kCifWidth, kCifHeight);
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{50, 30, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {15, 8, 75, 2, 2, 2, 2, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{28, 25, 0.70, 0.62}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestLibaom, HdAV1) {
  auto config = CreateHdConfig();
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kHdWidth, kHdHeight);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{1000, 50, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 3, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36.5, 35, 0.94, 0.92}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

}  // namespace
}  // namespace test
}  // namespace webrtc
