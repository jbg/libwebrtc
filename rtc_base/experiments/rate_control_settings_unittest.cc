/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/rate_control_settings.h"

#include "api/video_codecs/video_codec.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

namespace {

using test::ExplicitKeyValueConfig;
using ::testing::DoubleEq;
using ::testing::Optional;

TEST(RateControlSettingsTest, CongestionWindow) {
  EXPECT_TRUE(
      RateControlSettings(ExplicitKeyValueConfig("")).UseCongestionWindow());

  const RateControlSettings settings_after(
      ExplicitKeyValueConfig("WebRTC-CongestionWindow/QueueSize:100/"));
  EXPECT_TRUE(settings_after.UseCongestionWindow());
  EXPECT_EQ(settings_after.GetCongestionWindowAdditionalTimeMs(), 100);
}

TEST(RateControlSettingsTest, CongestionWindowPushback) {
  EXPECT_TRUE(RateControlSettings(ExplicitKeyValueConfig(""))
                  .UseCongestionWindowPushback());

  const RateControlSettings settings_after(ExplicitKeyValueConfig(
      "WebRTC-CongestionWindow/QueueSize:100,MinBitrate:100000/"));
  EXPECT_TRUE(settings_after.UseCongestionWindowPushback());
  EXPECT_EQ(settings_after.CongestionWindowMinPushbackTargetBitrateBps(),
            100000u);
}

TEST(RateControlSettingsTest, CongestionWindowPushbackDropframe) {
  EXPECT_TRUE(RateControlSettings(ExplicitKeyValueConfig(""))
                  .UseCongestionWindowPushback());

  const RateControlSettings settings_after(ExplicitKeyValueConfig(
      "WebRTC-CongestionWindow/"
      "QueueSize:100,MinBitrate:100000,DropFrame:true/"));
  EXPECT_TRUE(settings_after.UseCongestionWindowPushback());
  EXPECT_EQ(settings_after.CongestionWindowMinPushbackTargetBitrateBps(),
            100000u);
  EXPECT_TRUE(settings_after.UseCongestionWindowDropFrameOnly());
}

TEST(RateControlSettingsTest, CongestionWindowPushbackDefaultConfig) {
  const RateControlSettings settings(ExplicitKeyValueConfig(""));
  EXPECT_TRUE(settings.UseCongestionWindowPushback());
  EXPECT_EQ(settings.CongestionWindowMinPushbackTargetBitrateBps(), 30000u);
  EXPECT_TRUE(settings.UseCongestionWindowDropFrameOnly());
}

TEST(RateControlSettingsTest, PacingFactor) {
  EXPECT_FALSE(
      RateControlSettings(ExplicitKeyValueConfig("")).GetPacingFactor());

  EXPECT_THAT(
      RateControlSettings(
          ExplicitKeyValueConfig("WebRTC-VideoRateControl/pacing_factor:1.2/"))
          .GetPacingFactor(),
      Optional(DoubleEq(1.2)));
}

TEST(RateControlSettingsTest, AlrProbing) {
  EXPECT_FALSE(RateControlSettings(ExplicitKeyValueConfig("")).UseAlrProbing());

  EXPECT_TRUE(RateControlSettings(ExplicitKeyValueConfig(
                                      "WebRTC-VideoRateControl/alr_probing:1/"))
                  .UseAlrProbing());
}

TEST(RateControlSettingsTest, LibvpxVp8QpMax) {
  EXPECT_FALSE(
      RateControlSettings(ExplicitKeyValueConfig("")).LibvpxVp8QpMax());

  EXPECT_EQ(RateControlSettings(ExplicitKeyValueConfig(
                                    "WebRTC-VideoRateControl/vp8_qp_max:50/"))
                .LibvpxVp8QpMax(),
            50);
}

TEST(RateControlSettingsTest, DoesNotGetTooLargeLibvpxVp8QpMaxValue) {
  EXPECT_FALSE(
      RateControlSettings(
          ExplicitKeyValueConfig("WebRTC-VideoRateControl/vp8_qp_max:70/"))
          .LibvpxVp8QpMax());
}

TEST(RateControlSettingsTest, LibvpxVp8MinPixels) {
  EXPECT_FALSE(
      RateControlSettings(ExplicitKeyValueConfig("")).LibvpxVp8MinPixels());

  EXPECT_EQ(
      RateControlSettings(ExplicitKeyValueConfig(
                              "WebRTC-VideoRateControl/vp8_min_pixels:50000/"))
          .LibvpxVp8MinPixels(),
      50000);
}

TEST(RateControlSettingsTest, DoesNotGetTooSmallLibvpxVp8MinPixelValue) {
  EXPECT_FALSE(
      RateControlSettings(
          ExplicitKeyValueConfig("WebRTC-VideoRateControl/vp8_min_pixels:0/"))
          .LibvpxVp8MinPixels());
}

TEST(RateControlSettingsTest, LibvpxTrustedRateController) {
  const RateControlSettings settings_before(ExplicitKeyValueConfig(""));
  EXPECT_TRUE(settings_before.LibvpxVp8TrustedRateController());
  EXPECT_TRUE(settings_before.LibvpxVp9TrustedRateController());

  const RateControlSettings settings_after(ExplicitKeyValueConfig(
      "WebRTC-VideoRateControl/trust_vp8:0,trust_vp9:0/"));
  EXPECT_FALSE(settings_after.LibvpxVp8TrustedRateController());
  EXPECT_FALSE(settings_after.LibvpxVp9TrustedRateController());
}

TEST(RateControlSettingsTest, Vp8BaseHeavyTl3RateAllocationLegacyKey) {
  const RateControlSettings settings_before(ExplicitKeyValueConfig(""));
  EXPECT_FALSE(settings_before.Vp8BaseHeavyTl3RateAllocation());

  const RateControlSettings settings_after(ExplicitKeyValueConfig(
      "WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/"));
  EXPECT_TRUE(settings_after.Vp8BaseHeavyTl3RateAllocation());
}

TEST(RateControlSettingsTest,
     Vp8BaseHeavyTl3RateAllocationVideoRateControlKey) {
  const RateControlSettings settings_before(ExplicitKeyValueConfig(""));
  EXPECT_FALSE(settings_before.Vp8BaseHeavyTl3RateAllocation());

  const RateControlSettings settings_after(ExplicitKeyValueConfig(
      "WebRTC-VideoRateControl/vp8_base_heavy_tl3_alloc:1/"));
  EXPECT_TRUE(settings_after.Vp8BaseHeavyTl3RateAllocation());
}

TEST(RateControlSettingsTest,
     Vp8BaseHeavyTl3RateAllocationVideoRateControlKeyOverridesLegacyKey) {
  const RateControlSettings settings_before(ExplicitKeyValueConfig(""));
  EXPECT_FALSE(settings_before.Vp8BaseHeavyTl3RateAllocation());

  const RateControlSettings settings_after(ExplicitKeyValueConfig(
      "WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/WebRTC-VideoRateControl/"
      "vp8_base_heavy_tl3_alloc:0/"));
  EXPECT_FALSE(settings_after.Vp8BaseHeavyTl3RateAllocation());
}

TEST(RateControlSettingsTest, UseEncoderBitrateAdjuster) {
  // Should be on by default.
  EXPECT_TRUE(RateControlSettings(ExplicitKeyValueConfig(""))
                  .UseEncoderBitrateAdjuster());

  // Can be turned off via field trial.
  EXPECT_FALSE(RateControlSettings(
                   ExplicitKeyValueConfig(
                       "WebRTC-VideoRateControl/bitrate_adjuster:false/"))
                   .UseEncoderBitrateAdjuster());
}

}  // namespace

}  // namespace webrtc
