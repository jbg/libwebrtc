/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_GAIN_ANALYZER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_GAIN_ANALYZER_H_

#include <memory>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// `EchoPathGainAnalyzer` monitors changes in echo path gain to determine
// convergence. Monitoring occurs during the call's initial phase and when echo
// path gain is sufficiently strong.
class EchoPathGainAnalyzer {
 public:
  explicit EchoPathGainAnalyzer(bool initial_consistent_status);
  void Update(rtc::ArrayView<const std::vector<float>> filters_time_domain,
              rtc::ArrayView<const int> filter_delay_blocks,
              bool active_render);
  bool consistent_echo_path_gain() const {
    return timeout_ || low_echo_path_gain_ || consistent_echo_path_gain_;
  }

 private:
  bool consistent_echo_path_gain_;
  bool timeout_ = false;
  bool low_echo_path_gain_ = false;
  int number_render_updates_ = 0;
  int number_updates_ = 0;
  float echo_path_gain_thr_ = 0.0;
  int next_potential_consistent_update_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_GAIN_ANALYZER_H_
