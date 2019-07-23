/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/yield_policy.h"

#include "absl/base/attributes.h"

namespace rtc {
namespace {
static YieldInterface* current_yield_policy = nullptr;
}

ScopedYieldPolicy::ScopedYieldPolicy(YieldInterface* policy)
    : previous_(current_yield_policy) {
  current_yield_policy = policy;
}

ScopedYieldPolicy::~ScopedYieldPolicy() {
  current_yield_policy = previous_;
}

void ScopedYieldPolicy::YieldExecution() {
  if (current_yield_policy)
    current_yield_policy->YieldExecution();
}

bool ScopedYieldPolicy::Active() {
  return current_yield_policy != nullptr;
}

std::unique_ptr<EventInterface> ScopedYieldPolicy::CreateEvent(
    bool manual_reset,
    bool initially_signaled) {
  if (current_yield_policy)
    return current_yield_policy->CreateEvent(manual_reset, initially_signaled);
  else {
    return nullptr;
  }
}
}  // namespace rtc
