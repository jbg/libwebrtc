/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TASK_RUNNER_TASK_RUNNER_H_
#define TEST_TASK_RUNNER_TASK_RUNNER_H_
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_checker.h"
#include "test/task_runner/default_task_runner.h"
#include "test/task_runner/task_runner_interface.h"

namespace webrtc {
namespace task_runner_impl {
template <class Closure>
class TaskWrapper : public PendingTaskInterface {
 public:
  explicit TaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  void RunClosure() override { return closure_(); }
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};

template <class Closure>
class RepeatingTaskWrapper;

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)(Timestamp) const>
    : public RepeatingTaskInterface {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta RunClosure(Timestamp at_time) override { return closure_(at_time); }
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)() const>
    : public RepeatingTaskInterface {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta RunClosure(Timestamp at_time) override { return closure_(); }
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};

}  // namespace task_runner_impl

class TaskHandle {
 public:
  TaskHandle();
  ~TaskHandle();
  TaskHandle(TaskHandle&& other);
  TaskHandle& operator=(TaskHandle&& other);
  TaskHandle(const TaskHandle&) = delete;
  TaskHandle& operator=(const TaskHandle&) = delete;
  void Stop();
  void PostStop();
  bool IsRunning() const;

 private:
  friend class TaskRunner;
  explicit TaskHandle(RepeatingTaskHandleInterface* task);
  rtc::SequencedTaskChecker sequence_checker_;
  RepeatingTaskHandleInterface* task_ = nullptr;
};

class RTC_LOCKABLE TaskRunner {
 public:
  TaskRunner(TaskRunnerFactory* factory,
             absl::string_view queue_name,
             TaskQueuePriority priority = TaskQueuePriority::NORMAL)
      : impl_(factory->Create(queue_name, priority)) {}
  explicit TaskRunner(absl::string_view queue_name,
                      TaskQueuePriority priority = TaskQueuePriority::NORMAL)
      : impl_(new DefaultTaskRunner(queue_name, priority)) {}
  ~TaskRunner() { delete impl_; }
  TaskRunner(TaskRunner&& other) : impl_(other.impl_) { other.impl_ = nullptr; }
  TaskRunner(const TaskRunner&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;

  template <class Closure>
  void Invoke(Closure&& closure) {
    impl_->Invoke(absl::WrapUnique(new task_runner_impl::TaskWrapper<Closure>(
        std::forward<Closure>(closure))));
  }

  template <class Closure>
  void PostTask(Closure&& closure) {
    impl_->Post(TimeDelta::Zero(),
                absl::WrapUnique(new task_runner_impl::TaskWrapper<Closure>(
                    std::forward<Closure>(closure))));
  }

  template <class Closure>
  void PostDelayed(TimeDelta delay, Closure&& closure) {
    RTC_DCHECK(delay.IsFinite());
    RTC_DCHECK_GE(delay.ms(), 0);
    impl_->Post(delay,
                absl::WrapUnique(new task_runner_impl::TaskWrapper<Closure>(
                    std::forward<Closure>(closure))));
  }

  template <class Closure>
  TaskHandle Start(Closure&& closure) {
    auto* rep_task = impl_->Start(
        TimeDelta::Zero(),
        absl::WrapUnique(
            new task_runner_impl::RepeatingTaskWrapper<decltype(
                &Closure::operator())>(std::forward<Closure>(closure))));
    return TaskHandle(rep_task);
  }

  template <class Closure>
  TaskHandle DelayedStart(TimeDelta first_delay, Closure&& closure) {
    auto* rep_task = impl_->Start(
        first_delay,
        absl::WrapUnique(
            new task_runner_impl::RepeatingTaskWrapper<decltype(
                &Closure::operator())>(std::forward<Closure>(closure))));
    return TaskHandle(rep_task);
  }
  bool IsCurrent() const { return impl_->IsCurrent(); }

 private:
  TaskRunnerImplInterface* impl_;
};
}  // namespace webrtc

#endif  // TEST_TASK_RUNNER_TASK_RUNNER_H_
