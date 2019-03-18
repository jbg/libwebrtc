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
ABSL_CONST_INIT thread_local YieldInterface* current_yield_handler = nullptr;
}

ScopedYieldPolicy::ScopedYieldPolicy(YieldInterface* handler)
    : previous_(current_yield_handler) {
  current_yield_handler = handler;
}

ScopedYieldPolicy::~ScopedYieldPolicy() {
  current_yield_handler = previous_;
}

void ScopedYieldPolicy::Yield() {
  if (current_yield_handler)
    current_yield_handler->Yield();
}
}  // namespace rtc
