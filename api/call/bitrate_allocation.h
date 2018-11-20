/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_CALL_BITRATE_ALLOCATION_H_
#define API_CALL_BITRATE_ALLOCATION_H_

#include <stdint.h>

namespace webrtc {
struct BitrateAllocationUpdate {
  uint32_t target_bitrate_bps;
  uint32_t link_capacity_bps;
  uint8_t fraction_loss;
  int64_t rtt;
  int64_t bwe_period_ms;
};
}  // namespace webrtc
#endif  // API_CALL_BITRATE_ALLOCATION_H_
