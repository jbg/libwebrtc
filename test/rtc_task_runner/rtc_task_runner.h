/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RTC_TASK_RUNNER_RTC_TASK_RUNNER_H_
#define TEST_RTC_TASK_RUNNER_RTC_TASK_RUNNER_H_
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_checker.h"
#include "test/rtc_task_runner/rtc_task_runner_interfaces.h"

namespace webrtc {
namespace task_runner_impl {
template <class Closure>
class TaskWrapper : public PendingTaskInterface {
 public:
  explicit TaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  void Run() override { return closure_(); }
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
  TimeDelta Run(Timestamp at_time) override { return closure_(at_time); }
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)() const>
    : public RepeatingTaskInterface {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(); }
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
  friend class RtcTaskRunner;
  explicit TaskHandle(RepeatingTaskHandleImplInterface* task);
  rtc::SequencedTaskChecker sequence_checker_;
  RepeatingTaskHandleImplInterface* task_ = nullptr;
};

class RTC_LOCKABLE RtcTaskRunner {
 public:
  RtcTaskRunner(RtcTaskRunnerFactory* factory,
                absl::string_view queue_name,
                TaskQueuePriority priority = TaskQueuePriority::NORMAL)
      : impl_(factory->Create(queue_name, priority)) {}
  ~RtcTaskRunner() { delete impl_; }
  RtcTaskRunner(RtcTaskRunner&& other) : impl_(other.impl_) {
    other.impl_ = nullptr;
  }
  RtcTaskRunner(const RtcTaskRunner&) = delete;
  RtcTaskRunner& operator=(const RtcTaskRunner&) = delete;

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
  TaskHandle Repeat(Closure&& closure) {
    auto* rep_task = impl_->Repeat(
        TimeDelta::Zero(),
        absl::WrapUnique(
            new task_runner_impl::RepeatingTaskWrapper<decltype(
                &Closure::operator())>(std::forward<Closure>(closure))));
    return TaskHandle(rep_task);
  }

  template <class Closure>
  TaskHandle RepeatDelayed(TimeDelta first_delay, Closure&& closure) {
    auto* rep_task = impl_->Repeat(
        first_delay,
        absl::WrapUnique(
            new task_runner_impl::RepeatingTaskWrapper<decltype(
                &Closure::operator())>(std::forward<Closure>(closure))));
    return TaskHandle(rep_task);
  }
  bool IsCurrent() const { return impl_->IsCurrent(); }

 private:
  // Owned here and destroyed in destructor. We don't want to use unique_ptr
  // here since that will set the pointer to nullptr before deleting, meaning
  // that tasks that are accessing the implementation via impl_ will crash.
  RtcTaskRunnerImplInterface* impl_;
};
}  // namespace webrtc

#endif  // TEST_RTC_TASK_RUNNER_RTC_TASK_RUNNER_H_
