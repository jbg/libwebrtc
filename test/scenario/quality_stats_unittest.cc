/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
VideoStreamConfig AnalyzerVideoConfig(VideoQualityAnalyzer* analyzer) {
  VideoStreamConfig config;
  config.encoder.codec = VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
  config.encoder.implementation =
      VideoStreamConfig::Encoder::Implementation::kSoftware;
  config.hooks.frame_pair_handlers = {analyzer->Handler()};
  return config;
}
}  // namespace

TEST(ScenarioAnalyzerTest, PsnrIsHighWhenNetworkIsGood) {
  VideoQualityAnalyzer analyzer;
  {
    Scenario s;
    NetworkNodeConfig good_network;
    good_network.simulation.bandwidth = DataRate::kbps(1000);
    auto route = s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                                {s.CreateSimulationNode(good_network)},
                                s.CreateClient("callee", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())});
    s.CreateVideoStream(route->forward(), AnalyzerVideoConfig(&analyzer));
    s.RunFor(TimeDelta::seconds(1));
  }
  EXPECT_GT(analyzer.stats().psnr.Mean(), 46);
}

TEST(ScenarioAnalyzerTest, PsnrIsLowWhenNetworkIsBad) {
  VideoQualityAnalyzer analyzer;
  {
    Scenario s;
    NetworkNodeConfig bad_network;
    bad_network.simulation.bandwidth = DataRate::kbps(100);
    bad_network.simulation.loss_rate = 0.02;
    auto route = s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                                {s.CreateSimulationNode(bad_network)},
                                s.CreateClient("callee", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())});

    s.CreateVideoStream(route->forward(), AnalyzerVideoConfig(&analyzer));
    s.RunFor(TimeDelta::seconds(2));
  }
  EXPECT_LT(analyzer.stats().psnr.Mean(), 40);
}
}  // namespace test
}  // namespace webrtc
