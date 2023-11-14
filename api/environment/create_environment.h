/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENVIRONMENT_CREATE_ENVIRONMENT_H_
#define API_ENVIRONMENT_CREATE_ENVIRONMENT_H_

#include <utility>

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"

namespace webrtc {

// Helper for concise way to create an environment.
// `CreateEnvironment(tool1, tool2, tool3)` is a shortcut to
// `EnvironmentFactory().With(tool1).With(tool2).With(tool3).Create()`
// In particular `CreateEnvironment()` creates a default environment.
template <typename... Tools>
Environment CreateEnvironment(Tools... tools);

//------------------------------------------------------------------------------
// Implementation details follow
//------------------------------------------------------------------------------

namespace webrtc_internal_create_environment {

inline void Feed(EnvironmentFactory& factory) {}

template <typename FirstTool, typename... Tools>
void Feed(EnvironmentFactory& factory, FirstTool first, Tools... tools) {
  factory.With(std::forward<FirstTool>(first));
  Feed(factory, std::forward<Tools>(tools)...);
}

}  // namespace webrtc_internal_create_environment

template <typename... Tools>
Environment CreateEnvironment(Tools... tools) {
  EnvironmentFactory f;
  webrtc_internal_create_environment::Feed(f, std::forward<Tools>(tools)...);
  return f.Create();
}

}  // namespace webrtc

#endif  // API_ENVIRONMENT_CREATE_ENVIRONMENT_H_
