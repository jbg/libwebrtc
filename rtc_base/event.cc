/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/event.h"
#include "rtc_base/checks.h"

#include <chrono>

namespace rtc {

Event::Event(bool manual_reset, bool initially_signaled)
    : is_manual_reset_(manual_reset),
      event_status_(initially_signaled) {
}

void Event::Set() {
  {
    std::unique_lock<std::mutex> lock(event_mutex_);
    event_status_ = true;
  }
  event_cond_.notify_all();
}

void Event::Reset() {
  {
    std::unique_lock<std::mutex> lock(event_mutex_);
    event_status_ = false;
  }
  event_cond_.notify_all();
}

bool Event::Wait(int milliseconds) {
  std::unique_lock<std::mutex> lock;
  if (milliseconds == 0) {
    lock = std::unique_lock<std::mutex>(event_mutex_, std::try_to_lock);
  } else {
    lock = std::unique_lock<std::mutex>(event_mutex_);
  }

  bool waited = false;
  if (lock.owns_lock()) {
    if (milliseconds == 0) {
      waited = event_status_;
    } else if (milliseconds == kForever) {
      event_cond_.wait(lock, [this]() -> bool { return event_status_; });
      waited = event_status_;
    } else {
      waited =
          event_cond_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                               [this]() -> bool { return event_status_; });
    }

    if (waited) {
      RTC_DCHECK(event_status_);
      if (!is_manual_reset_) {
        event_status_ = false;
      }
    }
  }

  return waited;
}

}  // namespace rtc
