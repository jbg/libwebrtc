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
#include <numeric>
#include <vector>

#include "api/test/create_videocodec_test_fixture.h"
#include "media/base/media_constants.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {
// Test clips settings.
constexpr int kNumFramesLong = 300;

VideoCodecTestFixture::Config CreateConfig(std::string filename) {
  VideoCodecTestFixture::Config config;
  config.filename = filename;
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFramesLong;
  config.measure_cpu = true;
  config.encode_in_real_time = true;
  config.use_single_core = false;
  config.decode = false;
  return config;
}

TEST(VideoCodecPerfTestAv1, Hd) {
  constexpr int kHdWidth = 1280;
  constexpr int kHdHeight = 720;
  auto config = CreateConfig("ConferenceMotion_1280_720_50");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kHdWidth, kHdHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{1000, 30, 0}};

  fixture->RunTest(rate_profiles, nullptr, nullptr, nullptr);
  const auto& frame_stats = fixture->GetStats().GetFrameStatistics();

  if (!frame_stats.empty()) {
    uint64_t encode_time_sum_us =
        std::accumulate(frame_stats.begin(), frame_stats.end(), 0ull,
                        [](uint64_t sum, const auto& item) {
                          return sum + item.encode_time_us;
                        });
    double encode_time_avg_ms =
        static_cast<double>(encode_time_sum_us) / frame_stats.size() / 1000.0;
    printf("encode_time: %.2f ms\n", encode_time_avg_ms);
  }
}

}  // namespace
}  // namespace test
}  // namespace webrtc
