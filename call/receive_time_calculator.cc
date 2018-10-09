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
    : max_packet_time_repair("maxrep", TimeDelta::ms(500)),
      system_stall_threshold("sysstall", TimeDelta::ms(5)) {
  std::string trial_string =
      field_trial::FindFullName(kBweReceiveTimeCorrection);
  ParseFieldTrial({&max_packet_time_repair, &system_stall_threshold},
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
  int64_t corrected_time_us =
      uncaught_initial_reset_us_ + safe_time_us - stall_time_us;

  // All repairs depend on variables being intialized
  if (last_packet_time_us_ > 0) {
    int64_t packet_time_delta_us = packet_time_us - last_packet_time_us_;
    int64_t system_time_delta_us = system_time_us - last_system_time_us_;

    // Special case where clock resets backwards during initial stall.
    total_system_time_passed_us_ += rtc::SafeMax(0, system_time_delta_us);
    if (system_time_delta_us < 0)
      total_system_time_passed_us_ += config_.system_stall_threshold->us();
    if (packet_time_delta_us < 0 &&
        total_system_time_passed_us_ < config_.system_stall_threshold->us()) {
      uncaught_initial_reset_us_ = -packet_time_delta_us;
      corrected_time_us += uncaught_initial_reset_us_;
    }

    // Detect resets inbetween clock readings in socket and app.
    bool forward_clock_reset = corrected_time_us < last_corrected_time_us_;
    bool backward_clock_reset = system_time_us < packet_time_us;

    // Harder case with backward clock reset during stall, the reset being
    // smaller than the stall. Compensate throughout the stall.
    bool sign_of_overestimation =
        corrected_time_us > last_corrected_time_us_ + packet_time_delta_us;
    bool sign_of_stall_start = packet_time_delta_us >= 0 &&
                               packet_time_delta_us < system_time_delta_us;
    if (sign_of_stall_start && sign_of_overestimation) {
      small_reset_during_stall_ = true;
    } else if (system_time_delta_us > config_.system_stall_threshold->us() ||
               !sign_of_overestimation) {
      small_reset_during_stall_ = false;
    }

    // If resets are detected, advance time by (capped) packet time increase.
    if (forward_clock_reset || backward_clock_reset ||
        small_reset_during_stall_) {
      corrected_time_us = last_corrected_time_us_ +
                          rtc::SafeClamp(packet_time_delta_us, 0,
                                         config_.max_packet_time_repair->us());
    }
  }

  last_corrected_time_us_ = corrected_time_us;
  last_packet_time_us_ = packet_time_us;
  last_system_time_us_ = system_time_us;
  return corrected_time_us;
}

}  // namespace webrtc
