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
#include "rtc_base/task_utils/repeating_task.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
using webrtc_repeating_task_impl::RepeatingTaskBase;

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

class QueuedRepeatingTaskWrapper : public RepeatingTaskBase,
                                   public RepeatingTaskHandleImplInterface {
 public:
  QueuedRepeatingTaskWrapper(rtc::TaskQueue* task_queue,
                             TimeDelta first_delay,
                             std::unique_ptr<RepeatingTaskInterface> task)
      : RepeatingTaskBase(task_queue, first_delay), task_(std::move(task)) {}
  // Implements RepeatingTaskBase interface
  TimeDelta RunClosure() override { return task_->Run(GetCurrentTime()); }
  ~QueuedRepeatingTaskWrapper() final = default;
  // Implements RepeatingTaskHandleImplInterface
  void Stop() final { RepeatingTaskBase::Stop(); }
  void PostStop() final { RepeatingTaskBase::PostStop(); }

 private:
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
  RTC_DCHECK(delay >= TimeDelta::Zero());
  delay = std::max(TimeDelta::Zero(), delay);
  auto repeating_task = absl::make_unique<QueuedRepeatingTaskWrapper>(
      &task_queue_, delay, std::move(task));
  auto* repeating_task_ptr = repeating_task.get();
  task_queue_.PostDelayedTask(std::move(repeating_task), delay.ms());
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
