/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker_impl.cc.

#include "rtc_base/thread_checker_impl.h"
#include "api/task_queue/task_queue_base.h"

namespace rtc {

ThreadCheckerImpl::ThreadCheckerImpl()
    : valid_thread_(CurrentThreadRef()),
      valid_queue_(webrtc::TaskQueueBase::Current()) {}

ThreadCheckerImpl::~ThreadCheckerImpl() {}

bool ThreadCheckerImpl::CalledSequentially() const {
  const void* current_queue = webrtc::TaskQueueBase::Current();
  const PlatformThreadRef current_thread = CurrentThreadRef();
  CritScope scoped_lock(&lock_);
  if (valid_queue_) {
    return valid_queue_ == current_queue;
  } else if (valid_thread_) {
    return !current_queue && IsThreadRefEqual(valid_thread_, current_thread);
  } else {  // Previously detached.
    valid_queue_ = current_queue;
    valid_thread_ = current_thread;
    return true;
  }
}

void ThreadCheckerImpl::Detach() {
  CritScope scoped_lock(&lock_);
  valid_queue_ = nullptr;
  valid_thread_ = 0;
}

}  // namespace rtc
