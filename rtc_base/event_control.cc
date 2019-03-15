/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/event_control.h"

#include "absl/base/attributes.h"

namespace rtc {
ABSL_CONST_INIT thread_local EventSyncInterface*
    ThreadScopedEventSync::current_sync_ = nullptr;

ThreadScopedEventSync::ThreadScopedEventSync(EventSyncInterface* event_sync)
    : previous_(current_sync_) {
  current_sync_ = event_sync;
}

ThreadScopedEventSync::~ThreadScopedEventSync() {
  current_sync_ = previous_;
}
}  // namespace rtc
