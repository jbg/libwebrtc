/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_path_gain_analyzer.h"

#include <array>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

TEST(EchoPathGainAnalyzer, ConsistentEchoPath) {
  EchoPathGainAnalyzer echo_path_gain_analyzer(
      /*initial_consistent_status=*/false);
  const float echo_path_gain = 0.1f;
  std::array<std::vector<float>, 1> h = {
      std::vector<float>(kBlockSize * 5, echo_path_gain)};
  std::array<int, 1> delay_block = {2};
  for (int k = 0; k < 2 * kNumBlocksPerSecond; ++k) {
    echo_path_gain_analyzer.Update(h, delay_block, /*active_render=*/true);
  }
  EXPECT_TRUE(echo_path_gain_analyzer.consistent_echo_path_gain());
}

TEST(EchoPathGainAnalyzer, NotConsistentEchoPath) {
  EchoPathGainAnalyzer echo_path_gain_analyzer(
      /*initial_consistent_status=*/false);
  float echo_path_gain = 0.01f;
  std::array<std::vector<float>, 1> h = {
      std::vector<float>(kBlockSize * 5, echo_path_gain)};
  std::array<int, 1> delay_block = {2};
  const float increase_gain_factor = 10.0f;
  for (int k = 0; k < 2 * kNumBlocksPerSecond; ++k) {
    if (k % (kNumBlocksPerSecond / 2)) {
      echo_path_gain *= increase_gain_factor;
    }
    std::fill(h[0].begin(), h[0].end(), echo_path_gain);
    echo_path_gain_analyzer.Update(h, delay_block, /*active_render*/ true);
  }
}

TEST(EchoPathGainAnalyzer, NotUpdateDuringNotActiveFarend) {
  EchoPathGainAnalyzer echo_path_gain_analyzer(
      /*initial_consistent_status=*/false);
  std::array<std::vector<float>, 1> h = {
      std::vector<float>(kBlockSize * 5, 0.1f)};
  std::array<int, 1> delay_block = {2};

  for (int k = 0; k < 2 * kNumBlocksPerSecond; ++k) {
    echo_path_gain_analyzer.Update(h, delay_block, /*active_render=*/false);
  }
  EXPECT_FALSE(echo_path_gain_analyzer.consistent_echo_path_gain());
}

}  // namespace

}  // namespace webrtc
