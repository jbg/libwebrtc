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

namespace webrtc {

SequenceCheckerImpl::SequenceCheckerImpl()
    : valid_thread_(rtc::CurrentThreadRef()),
      valid_queue_(webrtc::TaskQueueBase::Current()) {}

SequenceCheckerImpl::~SequenceCheckerImpl() = default;

bool SequenceCheckerImpl::IsCurrent() const {
  const TaskQueueBase* const current_queue = webrtc::TaskQueueBase::Current();
  const rtc::PlatformThreadRef current_thread = rtc::CurrentThreadRef();
  rtc::CritScope scoped_lock(&lock_);
  if (!valid_thread_) {  // Previously detached.
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
  valid_queue_ = nullptr;
  valid_thread_ = 0;
}

}  // namespace webrtc
