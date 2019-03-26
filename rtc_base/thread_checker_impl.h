/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker_impl.h.

#ifndef RTC_BASE_THREAD_CHECKER_IMPL_H_
#define RTC_BASE_THREAD_CHECKER_IMPL_H_

#include "rtc_base/critical_section.h"
#include "rtc_base/platform_thread_types.h"

namespace rtc {
namespace internal {
// Forward declaration of the internal implementation of RTC_GUARDED_BY().
// SequencedTaskChecker grants this class access to call its IsCurrent() method.
// See thread_checker.h for more details.
class AnnounceOnThread;
}  // namespace internal

// Real implementation of ThreadChecker, for use in debug mode, or
// for temporary use in release mode (e.g. to RTC_CHECK on a threading issue
// seen only in the wild).
//
// Note: You should almost always use the ThreadChecker class to get the
// right version for your build configuration.
class ThreadCheckerImpl {
 public:
  ThreadCheckerImpl();
  ~ThreadCheckerImpl();

  bool CalledSequentially() const;
  // TODO(srte): Remove this alias.
  bool CalledOnValidThread() const { return CalledSequentially(); }

  // Changes the task queue or thread that is checked for in IsCurrent and
  // CalledOnValidThread.  This may be useful when an object may be created on
  // one task queue / thread and then used exclusively on another thread.
  void Detach();
  // TODO(srte): Remove this alias.
  void DetachFromThread() { Detach(); }

 private:
  friend class internal::AnnounceOnThread;
  bool IsCurrent() const { return CalledSequentially(); }
  CriticalSection lock_;
  // This is mutable so that CalledOnValidThread can set it.
  // It's guarded by |lock_|.
  mutable PlatformThreadRef valid_thread_;
  mutable const void* valid_queue_;
};

}  // namespace rtc

#endif  // RTC_BASE_THREAD_CHECKER_IMPL_H_
