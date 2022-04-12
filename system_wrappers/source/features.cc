/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/features.h"

#include <memory>

#include "rtc_base/checks.h"

namespace webrtc {
namespace features {

namespace {

// This should be initialized by Init(), and be immutable afterwards.
std::unique_ptr<Overrides> g_overrides_;

}  // namespace

// Returns whether the features is currently enabled.
bool IsEnabled(const Feature& feature) {
  if (!g_overrides_) {
    return feature.default_state == Default::ENABLED;
  }
  if (g_overrides_->enabled_features.contains(feature.name)) {
    return true;
  }
  if (g_overrides_->disabled_features.contains(feature.name)) {
    return false;
  }
  return feature.default_state == Default::ENABLED;
}

// Returns the current value of a feature paramater.
std::string GetParamValue(const Feature& feature,
                          const std::string& param_name) {
  if (!g_overrides_) {
    return "";
  }
  std::string qualified_name = feature.name + ":" + param_name;
  auto it = g_overrides_->params.find(qualified_name);
  if (it == g_overrides_->params.end()) {
    return "";
  }
  return it->second;
}

void Init(std::unique_ptr<Overrides> overrides) {
  RTC_DCHECK(!g_overrides_);
  g_overrides_ = std::move(overrides);
}

}  // namespace features
}  // namespace webrtc
