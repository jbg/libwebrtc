/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/balanced_degradation_experiment.h"

#include <stdio.h>
#include <limits>
#include <string>

#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
constexpr char kFieldTrial[] = "WebRTC-Video-BalancedDegradation";
constexpr int kMinFps = 1;
constexpr int kMaxFps = 100;

std::vector<BalancedDegradationExperiment::Config> DefaultConfig() {
  return {{320 * 240, 7}, {480 * 270, 10}, {640 * 480, 15}};
}
}  // namespace

bool BalancedDegradationExperiment::Enabled() {
  return webrtc::field_trial::IsEnabled(kFieldTrial);
}

std::vector<BalancedDegradationExperiment::Config>
BalancedDegradationExperiment::GetConfigs() {
  if (!Enabled())
    return DefaultConfig();

  const std::string group = webrtc::field_trial::FindFullName(kFieldTrial);
  if (group.empty())
    return DefaultConfig();

  std::vector<Config> configs(3);
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d,%d,%d,%d", &(configs[0].pixels),
             &(configs[0].fps), &(configs[1].pixels), &(configs[1].fps),
             &(configs[2].pixels), &(configs[2].fps)) != 6) {
    RTC_LOG(LS_WARNING) << "Too few parameters provided.";
    return DefaultConfig();
  }

  for (const auto& config : configs) {
    if (config.fps < kMinFps || config.fps > kMaxFps) {
      RTC_LOG(LS_WARNING) << "Unsupported fps setting, value ignored.";
      return DefaultConfig();
    }
  }

  for (size_t i = 1; i < configs.size(); ++i) {
    if (configs[i].pixels < configs[i - 1].pixels ||
        configs[i].fps < configs[i - 1].fps) {
      RTC_LOG(LS_WARNING) << "Invalid parameter value provided.";
      return DefaultConfig();
    }
  }

  return configs;
}

int BalancedDegradationExperiment::MinFps(int pixels,
                                          const std::vector<Config>& configs) {
  for (const auto& config : configs) {
    if (pixels <= config.pixels) {
      return config.fps;
    }
  }
  return std::numeric_limits<int>::max();
}

int BalancedDegradationExperiment::MaxFps(int pixels,
                                          const std::vector<Config>& configs) {
  RTC_DCHECK_GT(configs.size(), 1);
  for (size_t i = 0; i < configs.size() - 1; ++i) {
    if (pixels <= configs[i].pixels) {
      return configs[i + 1].fps;
    }
  }
  return std::numeric_limits<int>::max();
}

}  // namespace webrtc
