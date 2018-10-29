/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/repeated_task.h"

namespace webrtc {
namespace webrtc_repeated_task_impl {
bool RepeatedTaskBase::Run() {
  RTC_DCHECK_RUN_ON(task_queue_);
  // Return true to tell the TaskQueue to destruct this object.
  if (!running_)
    return true;

  TimeDelta delay = RunClosure();
  RTC_DCHECK(delay.IsFinite());

  if (interval_mode_ == RepeatingTaskIntervalMode::kIncludingExecution) {
    TimeDelta lost_time = Timestamp::us(rtc::TimeMicros()) - next_run_time_;
    next_run_time_ += delay;
    delay -= lost_time;
  }
  // absl::WrapUnique lets us repost this task on the TaskQueue.
  if (delay >= TimeDelta::Zero()) {
    task_queue_->PostDelayedTask(absl::WrapUnique(this), delay.ms());
  } else {
    task_queue_->PostTask(absl::WrapUnique(this));
  }
  // Return false to tell the TaskQueue to not destruct this object since we
  // have taken ownership with absl::WrapUnique.
  return false;
}

RepeatedTaskBase::RepeatedTaskBase(rtc::TaskQueue* task_queue,
                                   RepeatingTaskIntervalMode interval_mode,
                                   Timestamp next_run_time)
    : task_queue_(task_queue),
      interval_mode_(interval_mode),
      next_run_time_(next_run_time) {}

RepeatedTaskBase::~RepeatedTaskBase() {
  RTC_DCHECK_RUN_ON(task_queue_);
}

void RepeatedTaskBase::Stop() {
  RTC_DCHECK_RUN_ON(task_queue_);
  RTC_DCHECK(running_);
  running_ = false;
}

void RepeatedTaskBase::PostStop() {
  if (task_queue_->IsCurrent()) {
    //  RTC_LOG(LS_INFO)<<""
  }
  task_queue_->PostTask([this] { Stop(); });
}
}  // namespace webrtc_repeated_task_impl

RepeatedTaskHandle::RepeatedTaskHandle(
    webrtc_repeated_task_impl::RepeatedTaskBase* repeated_task)
    : repeated_task_(repeated_task) {}

void RepeatedTaskHandle::Stop(std::unique_ptr<RepeatedTaskHandle> handle) {
  handle->repeated_task_->Stop();
}

void RepeatedTaskHandle::PostStop(std::unique_ptr<RepeatedTaskHandle> handle) {
  handle->repeated_task_->PostStop();
}

}  // namespace webrtc
