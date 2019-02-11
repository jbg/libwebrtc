/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/rtc_task_runner/rtc_task_runner.h"
namespace webrtc {
TaskHandle::TaskHandle(RepeatingTaskHandleImplInterface* task) : task_(task) {}

TaskHandle::TaskHandle() {
  sequence_checker_.Detach();
}

TaskHandle::~TaskHandle() {
  sequence_checker_.Detach();
}

TaskHandle::TaskHandle(TaskHandle&& other) : task_(other.task_) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  other.task_ = nullptr;
}

TaskHandle& TaskHandle::operator=(TaskHandle&& other) {
  RTC_DCHECK_RUN_ON(&other.sequence_checker_);
  {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    task_ = other.task_;
  }
  other.task_ = nullptr;
  return *this;
}

void TaskHandle::Stop() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (task_) {
    task_->Stop();
    task_ = nullptr;
  }
}

void TaskHandle::PostStop() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (task_) {
    task_->PostStop();
    task_ = nullptr;
  }
}

bool TaskHandle::IsRunning() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return task_ != nullptr;
}

}  // namespace webrtc
