/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_FEATURES_H_
#define SYSTEM_WRAPPERS_INCLUDE_FEATURES_H_

#include <map>
#include <memory>
#include <set>
#include <string>

namespace webrtc {

namespace features {

enum class Default {
  DISABLED,
  ENABLED,
};

struct Feature {
  constexpr Feature(const char* name, Default default_state)
      : name(name), default_state(default_state) {}
  // The name of the feature. This should be unique to each feature and is used
  // for enabling/disabling features via command line flags and experiments.
  // It is strongly recommended to use CamelCase style for feature names, e.g.
  // "MyGreatFeature".
  const char* const name;

  // The default state (i.e. enabled or disabled) for this feature.
  // NOTE: The actual runtime state may be different, due to a field trial or a
  // command line switch.
  const Default default_state;
};

// Describes the state of all non-default features and parameters
struct Overrides {
  std::set<std::string> enabled_features;
  std::set<std::string> disabled_features;
  // |params| should be a "FeatureName:ParamName" -> ParamValue map.
  std::map<std::string, std::string> params;
};

// Returns whether the features is currently enabled.
bool IsEnabled(const Feature& feature);

// Returns the current value of a feature paramater.
std::string GetParamValue(const Feature& feature,
                          const std::string& param_name);

// Optionally initializes features.
// This method can be called at most once before any other call into webrtc.
void Init(std::unique_ptr<Overrides> feature_list);

}  // namespace features

}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_FEATURES_H_
