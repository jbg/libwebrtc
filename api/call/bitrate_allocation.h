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
// BitrateAllocationUpdate provides information to allocated streams about their
// bitrate allocation. It originates from the BitrateAllocater class and is
// propagated from there.
struct BitrateAllocationUpdate {
  // The allocated target bitrate. Media streams should produce this amount of
  // data. (Note that this may include packet overhead depending on
  // configuration.
  uint32_t target_bitrate_bps;
  // The allocated part of the estimated link capacity. This is more stable than
  // the target as it's based on the underlying link capacity estimate. This
  // should be used to change encoder configuration when the cost of change is
  // high.
  uint32_t link_capacity_bps;
  uint8_t fraction_loss;
  int64_t rtt;
  // |bwe_period_ms| is deprecated, use the link capacity allocation instead.
  int64_t bwe_period_ms;
};
}  // namespace webrtc
#endif  // API_CALL_BITRATE_ALLOCATION_H_
