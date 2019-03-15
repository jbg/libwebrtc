/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/event_control.h"

#include "absl/base/attributes.h"

namespace rtc {
namespace {
ABSL_CONST_INIT thread_local YieldInterface* current_yielder = nullptr;
}

ThreadScopedEventSync::ThreadScopedEventSync(YieldInterface* event_sync)
    : previous_(current_yielder) {
  current_yielder = event_sync;
}

ThreadScopedEventSync::~ThreadScopedEventSync() {
  current_yielder = previous_;
}

void ThreadScopedEventSync::Yield() {
  if (current_yielder)
    current_yielder->Yield();
}
}  // namespace rtc
