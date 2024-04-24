/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_timeout.h"

#include <algorithm>
#include <cstdint>

#include "api/units/time_delta.h"
#include "net/dcsctp/public/dcsctp_options.h"

namespace dcsctp {

// https://datatracker.ietf.org/doc/html/rfc4960#section-15.
constexpr double kRtoAlpha = 0.125;
constexpr double kRtoBeta = 0.25;

RetransmissionTimeout::RetransmissionTimeout(const DcSctpOptions& options)
    : min_rto_(options.rto_min.ToTimeDelta()),
      max_rto_(options.rto_max.ToTimeDelta()),
      max_rtt_(options.rtt_max.ToTimeDelta()),
      min_rtt_variance_(options.min_rtt_variance.ToTimeDelta() / 8),
      srtt_(options.rto_initial.ToTimeDelta()),
      rto_(options.rto_initial.ToTimeDelta()) {}

void RetransmissionTimeout::ObserveRTT(webrtc::TimeDelta rtt) {
  // Unrealistic values will be skipped. If a wrongly measured (or otherwise
  // corrupt) value was processed, it could change the state in a way that would
  // take a very long time to recover.
  if (rtt < webrtc::TimeDelta::Zero() || rtt > max_rtt_) {
    return;
  }

  // https://tools.ietf.org/html/rfc4960#section-6.3.1.
  if (first_measurement_) {
    srtt_ = rtt;
    rtt_var_ = rtt / 2;
    first_measurement_ = false;
  } else {
    webrtc::TimeDelta rtt_diff = (srtt_ - rtt).Abs();
    rtt_var_ = (1 - kRtoBeta) * rtt_var_ + kRtoBeta * rtt_diff;
    srtt_ = (1 - kRtoAlpha) * srtt_ + kRtoAlpha * rtt;
  }

  if (rtt_var_ < min_rtt_variance_) {
    rtt_var_ = min_rtt_variance_;
  }

  rto_ = srtt_ + 4 * rtt_var_;

  // Clamp RTO between min and max.
  rto_ = std::min(std::max(rto_, min_rto_), max_rto_);
}
}  // namespace dcsctp
