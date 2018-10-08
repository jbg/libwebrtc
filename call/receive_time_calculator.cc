/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/receive_time_calculator.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
using ::webrtc::field_trial::IsEnabled;

const char kBweReceiveTimeCorrection[] = "WebRTC-Bwe-ReceiveTimeFix";
}  // namespace

ReceiveTimeCalculator::ReceiveTimeCalculator() {}

std::unique_ptr<ReceiveTimeCalculator>
ReceiveTimeCalculator::CreateFromFieldTrial() {
  if (!IsEnabled(kBweReceiveTimeCorrection))
    return nullptr;
  return absl::make_unique<ReceiveTimeCalculator>();
}

int64_t ReceiveTimeCalculator::ReconcileReceiveTimes(int64_t packet_time_us,
                                                     int64_t system_time_us,
                                                     int64_t safe_time_us) {
  // The delay should be positive. If not, system time was probably moved
  // backwards between reads in socket and here.
  int64_t time_delta_us = std::max(0l, system_time_us - packet_time_us);
  int64_t corrected_time_us =
      uncaught_initial_reset_us_ + safe_time_us - time_delta_us;

  // All repairs depend on variables being intialized
  if (last_packet_time_us_ > 0) {
    int64_t packet_time_skip_us = packet_time_us - last_packet_time_us_;
    int64_t system_skip_us = system_time_us - last_system_time_us_;

    // Save for case where clock resets downwards during initial stall.
    total_system_time_passed_us_ += system_skip_us;
    if (packet_time_skip_us < 0 &&
        total_system_time_passed_us_ < 50 * rtc::kNumMicrosecsPerMillisec) {
      uncaught_initial_reset_us_ = -packet_time_skip_us;
    }

    // Save for case where system time was moved forwards in between reads in
    // socket and app. Advance time by (capped) packet time increase.
    if (corrected_time_us < last_corrected_time_us_) {
      int64_t update_us = std::max(
          0l,
          std::min(100 * rtc::kNumMicrosecsPerMillisec, packet_time_skip_us));
      corrected_time_us = last_corrected_time_us_ + update_us;
    }

    // Save for the easily detectable case where system time became smaller than
    // packet time due to reset between the two reads. Advance time by packet
    // time increase.
    if (packet_time_skip_us >= 0 && system_time_us < packet_time_us) {
      corrected_time_us = last_corrected_time_us_ + packet_time_skip_us;
    }

    // Save for the harder case where system time was moved backwards in between
    // reads in socket and app right before a stall. Advance time by packet time
    // increase. Track case as state until stall ends.
    if (system_skip_us > 0)
      negative_skip_in_stall_state_ = false;
    if (packet_time_skip_us >= 0 && system_skip_us >= 0 &&
        (negative_skip_in_stall_state_ ||
         packet_time_skip_us < system_skip_us) &&
        corrected_time_us > last_corrected_time_us_ + packet_time_skip_us) {
      corrected_time_us = last_corrected_time_us_ + packet_time_skip_us;
      negative_skip_in_stall_state_ = true;
    }
  }

  last_corrected_time_us_ = corrected_time_us;
  last_packet_time_us_ = packet_time_us;
  last_system_time_us_ = system_time_us;
  return corrected_time_us;
}

}  // namespace webrtc
