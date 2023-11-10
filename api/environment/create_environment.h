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

// Helper for concise way to create environment
// `CreateEnvironment(dep1, dep2, dep3)` is a shortcut to
// `EnvironmentFactory().With(dep1).With(dep2).With(dep3).Create()`
template <typename... Args>
Environment CreateEnvironment(Args... args);

//
// Below are implementation details
//
namespace webrtc_internal_create_environment {

inline void CreateEnvironmentWith(EnvironmentFactory& factory) {}

template <typename FirstArg, typename... Args>
void CreateEnvironmentWith(EnvironmentFactory& factory,
                           FirstArg first,
                           Args... args) {
  factory.With(std::forward<FirstArg>(first));
  CreateEnvironmentWith(factory, std::forward<Args>(args)...);
}

}  // namespace webrtc_internal_create_environment

template <typename... Args>
Environment CreateEnvironment(Args... args) {
  EnvironmentFactory factory;
  webrtc_internal_create_environment::CreateEnvironmentWith(
      factory, std::forward<Args>(args)...);
  return factory.Create();
}

}  // namespace webrtc

#endif  // API_ENVIRONMENT_CREATE_ENVIRONMENT_H_
