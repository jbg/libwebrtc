/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/sequence_checker.h"

#if defined(WEBRTC_MAC)
#include <dispatch/dispatch.h>
#endif

#include "api/task_queue/task_queue_base.h"

namespace webrtc {
namespace {
const void* GetCurrentQueue() {
  const void* current_queue = TaskQueueBase::Current();
#if defined(WEBRTC_MAC)
  // If we're not running on a TaskQueue, use the system dispatch queue
  // label as an identifier. This allows us to use SequencedChecker to check
  // calls that comes from system callbacks such as those from a capture device
  // even if the callbacks might come on different underlying threads. Note that
  // on other platforms, the equivalent calls will have to come from a
  // consistent thread as we will rely on the thread id rather than the queue.
  if (current_queue == nullptr)
    return dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL);
#endif
  return current_queue;
}
}  // namespace

SequenceCheckerImpl::SequenceCheckerImpl()
    : attached_(true),
      valid_thread_(rtc::CurrentThreadRef()),
      valid_queue_(GetCurrentQueue()) {}

SequenceCheckerImpl::~SequenceCheckerImpl() = default;

bool SequenceCheckerImpl::IsCurrent() const {
  const void* const current_queue = GetCurrentQueue();
  const rtc::PlatformThreadRef current_thread = rtc::CurrentThreadRef();
  rtc::CritScope scoped_lock(&lock_);
  if (!attached_) {  // Previously detached.
    attached_ = true;
    valid_thread_ = current_thread;
    valid_queue_ = current_queue;
    return true;
  }
  if (valid_queue_ || current_queue) {
    return valid_queue_ == current_queue;
  }
  return rtc::IsThreadRefEqual(valid_thread_, current_thread);
}

void SequenceCheckerImpl::Detach() {
  rtc::CritScope scoped_lock(&lock_);
  attached_ = false;
  valid_queue_ = nullptr;
  valid_thread_ = 0;
}

}  // namespace webrtc
