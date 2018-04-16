/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/bbr_factory.h"
#include <memory>
#include <string>

#include "modules/congestion_controller/bbr/bbr_network_controller.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
const char kBbrLogInterval[] = "WebRTC-BweBbrLogInterval";
TimeDelta GetLogInterval() {
  std::string trial_string = webrtc::field_trial::FindFullName(kBbrLogInterval);
  int custom_interval_ms_;
  if (sscanf(trial_string.c_str(), "Enabled,%d", &custom_interval_ms_) == 1) {
    RTC_LOG(LS_INFO) << "Using custom log interval: " << custom_interval_ms_;
    return TimeDelta::ms(custom_interval_ms_);
  }
  return TimeDelta::seconds(10);
}
}  // namespace
BbrNetworkControllerFactory::BbrNetworkControllerFactory() {}

std::unique_ptr<NetworkControllerInterface> BbrNetworkControllerFactory::Create(
    NetworkControllerConfig config) {
  return rtc::MakeUnique<bbr::BbrNetworkController>(config);
}

TimeDelta BbrNetworkControllerFactory::GetProcessInterval() const {
  return GetLogInterval();
}

}  // namespace webrtc
