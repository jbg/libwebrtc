/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_PTIME_CONTROLLER_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_PTIME_CONTROLLER_H_

#include <vector>

#include "absl/types/optional.h"
#include "modules/audio_coding/audio_network_adaptor/controller.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"

namespace webrtc {

class PtimeController final : public Controller {
 public:
  PtimeController(rtc::ArrayView<const int> encoder_frame_lengths_ms,
                  int min_payload_bitrate_bps,
                  bool use_stable_target_bitrate);

  void UpdateNetworkMetrics(const NetworkMetrics& network_metrics) override;

  void MakeDecision(AudioEncoderRuntimeConfig* config) override;

 private:
  int OverheadBps(int frame_length_ms) const;

  const std::vector<int> encoder_frame_lengths_ms_;
  const int min_payload_bitrate_bps_;
  const bool use_stable_target_bitrate_;

  absl::optional<int> uplink_bandiwdth_bps_;
  absl::optional<int> target_bitrate_bps_;
  absl::optional<int> overhead_bytes_per_packet_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_PTIME_CONTROLLER_H_
