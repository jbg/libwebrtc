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

#include <string>

#include "absl/memory/memory.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
using ::webrtc::field_trial::IsEnabled;

const char kBweReceiveTimeCorrection[] = "WebRTC-Bwe-ReceiveTimeFix";
}  // namespace

ReceiveTimeCalculatorConfig::ReceiveTimeCalculatorConfig()
    : max_packet_time_repair("maxrep", TimeDelta::ms(2000)),
      stall_threshold("stall", TimeDelta::ms(5)),
      tolerance("tol", TimeDelta::ms(1)) {
  std::string trial_string =
      field_trial::FindFullName(kBweReceiveTimeCorrection);
  ParseFieldTrial({&max_packet_time_repair, &stall_threshold, &tolerance},
                  trial_string);
}
ReceiveTimeCalculatorConfig::ReceiveTimeCalculatorConfig(
    const ReceiveTimeCalculatorConfig&) = default;
ReceiveTimeCalculatorConfig::~ReceiveTimeCalculatorConfig() = default;

ReceiveTimeCalculator::ReceiveTimeCalculator()
    : config_(ReceiveTimeCalculatorConfig()) {}

std::unique_ptr<ReceiveTimeCalculator>
ReceiveTimeCalculator::CreateFromFieldTrial() {
  if (!IsEnabled(kBweReceiveTimeCorrection))
    return nullptr;
  return absl::make_unique<ReceiveTimeCalculator>();
}

int64_t ReceiveTimeCalculator::ReconcileReceiveTimes(int64_t packet_time_us,
                                                     int64_t system_time_us,
                                                     int64_t safe_time_us) {
  // The stall should be positive. If not, system time was probably moved
  // backwards between reads in socket and here.
  int64_t stall_time_us = rtc::SafeMax(0, system_time_us - packet_time_us);
  int64_t corrected_time_us = safe_time_us - stall_time_us;

  // All repairs depend on variables being intialized
  if (last_packet_time_us_ > 0) {
    int64_t packet_time_delta_us = packet_time_us - last_packet_time_us_;
    int64_t system_time_delta_us = system_time_us - last_system_time_us_;
    int64_t safe_time_delta_us = safe_time_us - last_safe_time_us_;

    // Repair backwards clock resets during initial stall. In this case, the
    // reset is observed only in packet time but never in system time.
    if (system_time_delta_us < 0)
      total_system_time_passed_us_ += config_.stall_threshold->us();
    else
      total_system_time_passed_us_ += system_time_delta_us;
    if (packet_time_delta_us < 0 &&
        total_system_time_passed_us_ < config_.stall_threshold->us()) {
      static_clock_offset_us_ = -packet_time_delta_us;
    }
    corrected_time_us += static_clock_offset_us_;

    // Detect resets inbetween clock readings in socket and app.
    bool forward_clock_reset =
        corrected_time_us + config_.tolerance->us() < last_corrected_time_us_;
    bool large_backward_clock_reset = system_time_us < packet_time_us;

    // Harder case with backward clock reset during stall, the reset being
    // smaller than the stall. Compensate throughout the stall.
    bool small_backward_clock_reset =
        !large_backward_clock_reset &&
        safe_time_delta_us > system_time_delta_us + config_.tolerance->us();
    bool stall_start =
        packet_time_delta_us >= 0 &&
        system_time_delta_us > packet_time_delta_us + config_.tolerance->us();
    bool stall_is_over = safe_time_delta_us > config_.stall_threshold->us();
    bool packet_time_caught_up =
        corrected_time_us <= last_corrected_time_us_ + packet_time_delta_us +
                                 config_.tolerance->us();
    if (stall_start && small_backward_clock_reset)
      small_reset_during_stall_ = true;
    else if (stall_is_over || packet_time_caught_up)
      small_reset_during_stall_ = false;

    // If resets are detected, advance time by (capped) packet time increase.
    if (forward_clock_reset || large_backward_clock_reset ||
        small_reset_during_stall_) {
      corrected_time_us = last_corrected_time_us_ +
                          rtc::SafeClamp(packet_time_delta_us, 0,
                                         config_.max_packet_time_repair->us());
    }
  }

  last_corrected_time_us_ = corrected_time_us;
  last_packet_time_us_ = packet_time_us;
  last_system_time_us_ = system_time_us;
  last_safe_time_us_ = safe_time_us;
  return corrected_time_us;
}

}  // namespace webrtc
