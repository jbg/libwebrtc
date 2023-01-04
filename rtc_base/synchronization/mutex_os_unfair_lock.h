/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_OS_UNFAIR_LOCK_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_OS_UNFAIR_LOCK_H_

#include <os/lock.h>

#include "absl/base/attributes.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE MutexImpl final {
 public:
  MutexImpl() = default;
  MutexImpl(const MutexImpl&) = delete;
  MutexImpl& operator=(const MutexImpl&) = delete;

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() { os_unfair_lock_lock(&lock_); }
  ABSL_MUST_USE_RESULT bool TryLock() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return os_unfair_lock_trylock(&lock_);
  }
  void AssertHeld() const RTC_ASSERT_EXCLUSIVE_LOCK() {
#if RTC_DCHECK_IS_ON
    os_unfair_lock_assert_owner(&lock_);
#endif
  }
  void Unlock() RTC_UNLOCK_FUNCTION() { os_unfair_lock_unlock(&lock_); }

 private:
  os_unfair_lock lock_ = OS_UNFAIR_LOCK_INIT;
};

}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_OS_UNFAIR_LOCK_H_
