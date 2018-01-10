/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EVENT_H_
#define RTC_BASE_EVENT_H_

#include "rtc_base/constructormagic.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace rtc {

class Event {
 public:
  static const int kForever = -1;

  Event(bool manual_reset, bool initially_signaled);
  ~Event();

  void Set();
  void Reset();

  // Wait for the event to become signaled, for the specified number of
  // |milliseconds|.  To wait indefinetly, pass kForever.
  bool Wait(int milliseconds);

 private:
  const bool is_manual_reset_;
  std::atomic<bool> event_status_;

  std::mutex event_mutex_;
  std::condition_variable event_cond_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Event);
};

}  // namespace rtc

#endif  // RTC_BASE_EVENT_H_
