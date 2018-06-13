/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/simulcast.h"

#include "media/engine/constants.h"
#include "rtc_base/gunit.h"
#include "test/field_trial.h"

namespace webrtc {
namespace {
constexpr int kQpMax = 55;
constexpr double kBitratePriority = 2.0;
constexpr int kMaxFps = 33;
constexpr bool kScreenshare = true;
constexpr int kDefaultTemporalLayers = 3;  // Value from simulcast.cc.

// Values from kSimulcastConfigs in simulcast.cc.
const std::vector<webrtc::VideoStream> GetSimulcastBitrates720p() {
  std::vector<webrtc::VideoStream> layers(3);
  layers[0].min_bitrate_bps = 30000;
  layers[0].target_bitrate_bps = 150000;
  layers[0].max_bitrate_bps = 200000;
  layers[1].min_bitrate_bps = 150000;
  layers[1].target_bitrate_bps = 500000;
  layers[1].max_bitrate_bps = 700000;
  layers[2].min_bitrate_bps = 600000;
  layers[2].target_bitrate_bps = 2500000;
  layers[2].max_bitrate_bps = 2500000;
  return layers;
}

// Values from simulcast.cc.
const std::vector<webrtc::VideoStream> GetScreenshareBitrates() {
  std::vector<webrtc::VideoStream> layers(1);
  layers[0].min_bitrate_bps = cricket::kMinVideoBitrateBps;
  layers[0].target_bitrate_bps = 200000;
  layers[0].max_bitrate_bps = 1000000;
  return layers;
}

// Values from simulcast.cc.
const std::vector<webrtc::VideoStream> GetScreenshareSimulcastBitrates() {
  std::vector<webrtc::VideoStream> layers(2);
  layers[0].min_bitrate_bps = cricket::kMinVideoBitrateBps;
  layers[0].target_bitrate_bps = 200000;
  layers[0].max_bitrate_bps = 1000000;
  layers[1].min_bitrate_bps = 400000;
  layers[1].target_bitrate_bps = 1000000;
  layers[1].max_bitrate_bps = 1000000;
  return layers;
}
}  // namespace

TEST(SimulcastTest, ZeroBitrateForNoStreams) {
  std::vector<VideoStream> streams;
  EXPECT_EQ(0, cricket::GetTotalMaxBitrateBps(streams));
}

TEST(SimulcastTest, GetTotalMaxBitrateForOneStream) {
  std::vector<VideoStream> streams(1);
  streams[0].max_bitrate_bps = 100000;
  EXPECT_EQ(100000, cricket::GetTotalMaxBitrateBps(streams));
}

TEST(SimulcastTest, GetTotalMaxBitrateForThreeStreams) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;
  EXPECT_EQ(700000, cricket::GetTotalMaxBitrateBps(streams));
}

TEST(SimulcastTest, BoostMaxSimulcastLayerForNoStreams) {
  std::vector<VideoStream> streams;
  const int kMaxBitrateBps = 100000;
  cricket::BoostMaxSimulcastLayer(kMaxBitrateBps, &streams);
}

TEST(SimulcastTest, NoBandwidthAboveTotalMaxBitrateToGiveToMaxStream) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;

  // No bitrate above the total max to give to the highest stream.
  const int kMaxBitrateBps = cricket::GetTotalMaxBitrateBps(streams);
  cricket::BoostMaxSimulcastLayer(kMaxBitrateBps, &streams);
  EXPECT_EQ(400000, streams[2].max_bitrate_bps);
  EXPECT_EQ(kMaxBitrateBps, cricket::GetTotalMaxBitrateBps(streams));
}

TEST(SimulcastTest, BandwidthAboveTotalMaxBitrateGivenToMaxStream) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;

  // The bitrate above the total max should be given to the highest stream.
  const int kMaxBitrateBps = cricket::GetTotalMaxBitrateBps(streams) + 1;
  cricket::BoostMaxSimulcastLayer(kMaxBitrateBps, &streams);
  EXPECT_EQ(400000 + 1, streams[2].max_bitrate_bps);
  EXPECT_EQ(kMaxBitrateBps, cricket::GetTotalMaxBitrateBps(streams));
}

TEST(SimulcastTest, GetSimulcastConfig) {
  const std::vector<webrtc::VideoStream> kDefault = GetSimulcastBitrates720p();

  int kMaxBitrateBps = 0;
  size_t kMaxLayers = 3;
  std::vector<webrtc::VideoStream> streams = cricket::GetSimulcastConfig(
      kMaxLayers, 1280, 720, kMaxBitrateBps, kBitratePriority, kQpMax, kMaxFps,
      !kScreenshare);

  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
  EXPECT_EQ(1280u, streams[2].width);
  EXPECT_EQ(720u, streams[2].height);

  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(kDefaultTemporalLayers, streams[i].num_temporal_layers);
    EXPECT_EQ(kMaxFps, streams[i].max_framerate);
    EXPECT_EQ(kQpMax, streams[i].max_qp);
    EXPECT_EQ(kDefault[i].min_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_EQ(kDefault[i].target_bitrate_bps, streams[i].target_bitrate_bps);
    EXPECT_EQ(kDefault[i].max_bitrate_bps, streams[i].max_bitrate_bps);
    EXPECT_TRUE(streams[i].active);
  }
  // Currently set on lowest stream.
  EXPECT_EQ(kBitratePriority, streams[0].bitrate_priority);
  EXPECT_FALSE(streams[1].bitrate_priority);
  EXPECT_FALSE(streams[2].bitrate_priority);
}

TEST(SimulcastTest, GetSimulcastConfigWithLimitedMaxLayers) {
  int kMaxBitrateBps = 0;
  size_t kMaxLayers = 2;
  std::vector<webrtc::VideoStream> streams = cricket::GetSimulcastConfig(
      kMaxLayers, 1280, 720, kMaxBitrateBps, kBitratePriority, kQpMax, kMaxFps,
      !kScreenshare);

  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
}

TEST(SimulcastTest, GetSimulcastConfigWithNormalizedResolution) {
  int kMaxBitrateBps = 0;
  size_t kMaxLayers = 2;
  std::vector<webrtc::VideoStream> streams = cricket::GetSimulcastConfig(
      kMaxLayers, 640 + 1, 360 + 1, kMaxBitrateBps, kBitratePriority, kQpMax,
      kMaxFps, !kScreenshare);

  // Must be dividable by |2 ^ (num_layers - 1)|.
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
}

TEST(SimulcastTest, GetSimulcastConfigForScreenshare) {
  const std::vector<webrtc::VideoStream> kDefault = GetScreenshareBitrates();

  int kMaxBitrateBps = 0;
  size_t kMaxLayers = 3;
  std::vector<webrtc::VideoStream> streams = cricket::GetSimulcastConfig(
      kMaxLayers, 1280, 720, kMaxBitrateBps, kBitratePriority, kQpMax, kMaxFps,
      kScreenshare);

  EXPECT_EQ(1u, streams.size());
  EXPECT_EQ(1280u, streams[0].width);
  EXPECT_EQ(720u, streams[0].height);
  EXPECT_EQ(kQpMax, streams[0].max_qp);
  EXPECT_TRUE(streams[0].active);
  EXPECT_EQ(kBitratePriority, streams[0].bitrate_priority);
  EXPECT_EQ(kDefault[0].min_bitrate_bps, streams[0].min_bitrate_bps);
  EXPECT_EQ(kDefault[0].target_bitrate_bps, streams[0].target_bitrate_bps);
  EXPECT_EQ(kDefault[0].max_bitrate_bps, streams[0].max_bitrate_bps);
}

TEST(SimulcastTest, GetSimulcastConfigForScreenshareSimulcast) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-SimulcastScreenshare/Enabled/");

  const std::vector<webrtc::VideoStream> kDefault =
      GetScreenshareSimulcastBitrates();

  int kMaxBitrateBps = 0;
  size_t kMaxLayers = 3;
  std::vector<webrtc::VideoStream> streams = cricket::GetSimulcastConfig(
      kMaxLayers, 1280, 720, kMaxBitrateBps, kBitratePriority, kQpMax, kMaxFps,
      kScreenshare);

  EXPECT_EQ(2u, streams.size());
  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(1280u, streams[i].width);
    EXPECT_EQ(720u, streams[i].height);
    EXPECT_EQ(kDefault[i].min_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_EQ(kDefault[i].target_bitrate_bps, streams[i].target_bitrate_bps);
    EXPECT_EQ(kDefault[i].max_bitrate_bps, streams[i].max_bitrate_bps);
  }
}

}  // namespace webrtc
