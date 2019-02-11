/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/rtc_task_runner/default_task_runner.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
Timestamp GetCurrentTime() {
  return Timestamp::us(Clock::GetRealTimeClock()->TimeInMicroseconds());
}

class QueuedTaskWrapper : public QueuedTask {
 public:
  explicit QueuedTaskWrapper(std::unique_ptr<PendingTaskInterface> task)
      : task_(std::move(task)) {}
  bool Run() override {
    task_->Run();
    return true;
  }

 private:
  std::unique_ptr<PendingTaskInterface> task_;
};

class QueuedRepeatingTaskWrapper : public QueuedTask,
                                   public RepeatingTaskHandleImplInterface {
 public:
  QueuedRepeatingTaskWrapper(rtc::TaskQueue* task_queue,
                             TimeDelta first_delay,
                             std::unique_ptr<RepeatingTaskInterface> task)
      : task_queue_(task_queue),
        next_run_time_(GetCurrentTime() + first_delay),
        task_(std::move(task)) {}
  bool Run() final {
    RTC_DCHECK_RUN_ON(task_queue_);
    // Return true to tell the TaskQueue to destruct this object.
    if (next_run_time_.IsPlusInfinity())
      return true;
    TimeDelta delay = task_->Run(next_run_time_);
    // The closure might have stopped this task, in which case we return true to
    // destruct this object.
    if (next_run_time_.IsPlusInfinity())
      return true;

    RTC_DCHECK(delay.IsFinite());

    TimeDelta lost_time = GetCurrentTime() - next_run_time_;
    next_run_time_ += delay;
    delay -= lost_time;
    delay = std::max(TimeDelta::Zero(), delay);
    task_queue_->PostDelayedTask(absl::WrapUnique(this), delay.ms());

    // Return false to tell the TaskQueue to not destruct this object since we
    // have taken ownership with absl::WrapUnique.
    return false;
  }
  void Stop() final {
    RTC_DCHECK_RUN_ON(task_queue_);
    RTC_DCHECK(next_run_time_.IsFinite());
    next_run_time_ = Timestamp::PlusInfinity();
  }
  void PostStop() final {
    if (task_queue_->IsCurrent()) {
      RTC_DLOG(LS_INFO) << "Using PostStop() from the task queue running the "
                           "repeated task. Consider calling Stop() instead.";
    }
    task_queue_->PostTask([this] {
      RTC_DCHECK_RUN_ON(task_queue_);
      Stop();
    });
  }

 private:
  rtc::TaskQueue* const task_queue_;
  // This is always finite, except for the special case where it's PlusInfinity
  // to signal that the task should stop.
  Timestamp next_run_time_ RTC_GUARDED_BY(task_queue_);
  std::unique_ptr<RepeatingTaskInterface> task_;
};
}  // namespace

void DefaultTaskRunner::Invoke(std::unique_ptr<PendingTaskInterface> task) {
  rtc::Event done;
  task_queue_.PostTask([&done, &task] {
    task->Run();
    done.Set();
  });
  done.Wait(rtc::Event::kForever);
}

void DefaultTaskRunner::Post(TimeDelta delay,
                             std::unique_ptr<PendingTaskInterface> task) {
  if (delay <= delay.Zero()) {
    task_queue_.PostTask(absl::make_unique<QueuedTaskWrapper>(std::move(task)));
  } else {
    task_queue_.PostDelayedTask(
        absl::make_unique<QueuedTaskWrapper>(std::move(task)), delay.ms());
  }
}

RepeatingTaskHandleImplInterface* DefaultTaskRunner::Repeat(
    TimeDelta delay,
    std::unique_ptr<RepeatingTaskInterface> task) {
  auto repeating_task = absl::make_unique<QueuedRepeatingTaskWrapper>(
      &task_queue_, std::max(TimeDelta::Zero(), delay), std::move(task));
  auto* repeating_task_ptr = repeating_task.get();
  if (delay <= TimeDelta::Zero()) {
    task_queue_.PostTask(std::move(repeating_task));
  } else {
    task_queue_.PostDelayedTask(std::move(repeating_task), delay.ms());
  }
  return repeating_task_ptr;
}

bool DefaultTaskRunner::IsCurrent() const {
  return task_queue_.IsCurrent();
}

Clock* DefaultTaskRunnerFactory::GetClock() {
  return Clock::GetRealTimeClock();
}

void DefaultTaskRunnerFactory::Wait(TimeDelta duration) {
  rtc::Event done;
  done.Wait(duration.ms<int>());
}

RtcTaskRunnerImplInterface* DefaultTaskRunnerFactory::Create(
    absl::string_view queue_name,
    TaskQueuePriority priority) {
  return new DefaultTaskRunner(queue_name, priority);
}

}  // namespace webrtc
