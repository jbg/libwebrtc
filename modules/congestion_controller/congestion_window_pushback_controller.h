/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CONGESTION_WINDOW_PUSHBACK_CONTROLLER_H
#define CONGESTION_WINDOW_PUSHBACK_CONTROLLER_H

#include "api/transport/network_types.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/format_macros.h"

namespace webrtc {

class CongestionWindowPushbackController {
 public:
  CongestionWindowPushbackController();
  void UpdateOutstandingData(size_t outstanding_bytes);
  void UpdateMaxOutstandingData(size_t max_outstanding_bytes);
  uint32_t UpdateTargetBitrate(uint32_t bitrate_bps);
  void SetDataWindow(DataSize data_window);

 private:
  absl::optional<DataSize> current_data_window_;
  size_t outstanding_bytes_ = 0;
  uint32_t min_pushback_target_bitrate_bps_;
  double encoding_rate_ratio_ = 1.0;
};

}  // namespace webrtc

#endif  // CONGESTION_WINDOW_PUSHBACK_CONTROLLER_H
