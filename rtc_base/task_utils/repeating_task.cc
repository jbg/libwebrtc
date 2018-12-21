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
bool RepeatingTaskBase::Run() {
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

RepeatingTaskBase::RepeatingTaskBase(rtc::TaskQueue* task_queue,
                                     RepeatingTaskIntervalMode interval_mode,
                                     Timestamp next_run_time)
    : task_queue_(task_queue),
      interval_mode_(interval_mode),
      next_run_time_(next_run_time) {}

RepeatingTaskBase::~RepeatingTaskBase() = default;

void RepeatingTaskBase::Stop() {
  RTC_DCHECK(running_);
  running_ = false;
}

void RepeatingTaskBase::PostStop() {
  if (task_queue_->IsCurrent())
    RTC_LOG(LS_INFO) << "Using PostStop when Stop would work.";
  task_queue_->PostTask([this] {
    RTC_DCHECK_RUN_ON(task_queue_);
    Stop();
  });
}
}  // namespace webrtc_repeating_task_impl

RepeatingTask::RepeatingTask() = default;

RepeatingTask::RepeatingTask(
    webrtc_repeating_task_impl::RepeatingTaskBase* repeating_task)
    : repeating_task_(repeating_task) {}

RepeatingTask::RepeatingTask(RepeatingTask&& other)
    : repeating_task_(other.repeating_task_) {
  other.repeating_task_ = nullptr;
}

RepeatingTask& RepeatingTask::operator=(RepeatingTask&& other) {
  RTC_DCHECK(!repeating_task_);
  repeating_task_ = other.repeating_task_;
  other.repeating_task_ = nullptr;
  return *this;
}

webrtc::RepeatingTask::operator bool() const {
  return repeating_task_ != nullptr;
}

void RepeatingTask::Stop() {
  RTC_CHECK(repeating_task_);
  RTC_DCHECK_RUN_ON(repeating_task_->task_queue_);
  repeating_task_->Stop();
  repeating_task_ = nullptr;
}

void RepeatingTask::PostStop() {
  RTC_CHECK(repeating_task_);
  repeating_task_->PostStop();
  repeating_task_ = nullptr;
}

}  // namespace webrtc
