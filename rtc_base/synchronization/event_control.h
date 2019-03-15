/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYNCHRONIZATION_EVENT_CONTROL_H_
#define RTC_BASE_SYNCHRONIZATION_EVENT_CONTROL_H_

namespace rtc {
class YieldInterface {
 public:
  virtual ~YieldInterface() = default;
  virtual void Yield() = 0;
};

class ThreadScopedEventSync {
 public:
  explicit ThreadScopedEventSync(YieldInterface* event_sync);
  ~ThreadScopedEventSync();
  // Will yield as specified by the currently active thread-local yield policy
  // (which by default is a no-op).
  static void Yield();

 private:
  YieldInterface* const previous_;
};

}  // namespace rtc

#endif  // RTC_BASE_SYNCHRONIZATION_EVENT_CONTROL_H_
