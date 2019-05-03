/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_EXPERIMENT_H_

#include <vector>

namespace webrtc {

class BalancedDegradationExperiment {
 public:
  struct Config {
    bool operator==(const Config& o) const {
      return pixels == o.pixels && fps == o.fps;
    }

    int pixels;  // The video frame size.
    int fps;     // The framerate to be used if the frame size is less than or
                 // equal to |pixels|.
  };

  // Returns true if the experiment is enabled.
  static bool Enabled();

  // Returns configurations from field trial on success (default on failure).
  static std::vector<Config> GetConfigs();

  // Gets the min/max framerate from the |configs| based on |pixels|.
  static int MinFps(int pixels, const std::vector<Config>& configs);
  static int MaxFps(int pixels, const std::vector<Config>& configs);
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_EXPERIMENT_H_
