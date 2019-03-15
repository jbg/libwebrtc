/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EVENT_CONTROL_H_
#define RTC_BASE_EVENT_CONTROL_H_

namespace rtc {
class EventSyncInterface {
 public:
  virtual ~EventSyncInterface() = default;
  virtual void Yield() = 0;
};

class ThreadScopedEventSync {
 public:
  explicit ThreadScopedEventSync(EventSyncInterface* event_sync);
  ~ThreadScopedEventSync();

  inline static void MaybeYeild() {
    if (current_sync_)
      current_sync_->Yield();
  }

 private:
  static thread_local EventSyncInterface* current_sync_;
  EventSyncInterface* previous_;
};

}  // namespace rtc

#endif  // RTC_BASE_EVENT_CONTROL_H_
