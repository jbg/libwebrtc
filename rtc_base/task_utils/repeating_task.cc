/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_repeating_task_impl {
RepeatingTaskBase::RepeatingTaskBase(rtc::TaskQueue* task_queue,
                                     TimeDelta first_delay,
                                     IntervalMode interval_mode)
    : task_queue_(task_queue),
      interval_mode_(interval_mode),
      next_run_time_(Timestamp::us(rtc::TimeMicros()) + first_delay) {}

RepeatingTaskBase::~RepeatingTaskBase() = default;

bool RepeatingTaskBase::Run() {
  RTC_DCHECK_RUN_ON(task_queue_);
  // Return true to tell the TaskQueue to destruct this object.
  if (!running_)
    return true;

  TimeDelta delay = RunClosure();
  RTC_DCHECK(delay.IsFinite());

  if (interval_mode_ == IntervalMode::kIncludingExecution) {
    TimeDelta lost_time = Timestamp::us(rtc::TimeMicros()) - next_run_time_;
    next_run_time_ += delay;
    delay -= lost_time;
  }
  // absl::WrapUnique lets us repost this task on the TaskQueue.
  PostOrDelay(delay);
  // Return false to tell the TaskQueue to not destruct this object since we
  // have taken ownership with absl::WrapUnique.
  return false;
}

void RepeatingTaskBase::PostOrDelay(TimeDelta delay) {
  if (delay <= TimeDelta::Zero()) {
    task_queue_->PostTask(absl::WrapUnique(this));
  } else {
    task_queue_->PostDelayedTask(absl::WrapUnique(this), delay.ms());
  }
}

void RepeatingTaskBase::Stop() {
  RTC_DCHECK(running_);
  running_ = false;
}

void RepeatingTaskBase::PostStop() {
  if (task_queue_->IsCurrent()) {
    RTC_DLOG(LS_INFO) << "Using PostStop() from the task queue running the "
                         "repeated task. Consider calling Stop() instead.";
  }
  task_queue_->PostTask([this] {
    RTC_DCHECK_RUN_ON(task_queue_);
    Stop();
  });
}

}  // namespace webrtc_repeating_task_impl

RepeatingTask::RepeatingTask() {
  sequence_checker_.Detach();
}

bool RepeatingTask::Running() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return repeating_task_ != nullptr;
}

void RepeatingTask::Stop() {
  RTC_CHECK(repeating_task_);
  RTC_DCHECK_RUN_ON(repeating_task_->task_queue_);
  repeating_task_->Stop();
  {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    repeating_task_ = nullptr;
  }
}

void RepeatingTask::PostStop() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_CHECK(repeating_task_);
  repeating_task_->PostStop();
  repeating_task_ = nullptr;
}

}  // namespace webrtc
