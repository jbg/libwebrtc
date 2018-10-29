/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_REPEATED_TASK_H_
#define RTC_BASE_TASK_UTILS_REPEATED_TASK_H_

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
enum class RepeatingTaskIntervalMode {
  // Means the the interval is followed as close as possible, any extra delay
  // and execution time is compensated for.
  kIncludingExecution,
  // Means that the delay is followed, meaning, the task will repeat after the
  // given delay.
  kExcludingExecution
};

class RepeatedTaskHandle;

namespace webrtc_repeated_task_impl {
class RepeatedTaskBase : public rtc::QueuedTask {
 public:
  RepeatedTaskBase(rtc::TaskQueue* task_queue,
                   RepeatingTaskIntervalMode interval_mode,
                   Timestamp next_run_time);
  ~RepeatedTaskBase() override;
  virtual TimeDelta RunClosure() = 0;

 private:
  friend class ::webrtc::RepeatedTaskHandle;

  bool Run() final;
  void Stop();
  void PostStop();

  rtc::TaskQueue* const task_queue_;
  const RepeatingTaskIntervalMode interval_mode_;
  bool running_ RTC_GUARDED_BY(task_queue_) = true;
  Timestamp next_run_time_ RTC_GUARDED_BY(task_queue_);
};

// The template closure pattern is based on rtc::ClosureTask.
template <class Closure>
class RepeatedTaskImpl final : public RepeatedTaskBase {
 public:
  RepeatedTaskImpl(rtc::TaskQueue* task_queue,
                   Timestamp next_run_time,
                   RepeatingTaskIntervalMode interval_mode,
                   Closure&& closure)
      : RepeatedTaskBase(task_queue, interval_mode, next_run_time),
        closure_(std::forward<Closure>(closure)) {}

  TimeDelta RunClosure() override { return closure_(); }

 private:
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};
}  // namespace webrtc_repeated_task_impl

class RepeatedTaskHandle {
 public:
  RTC_DISALLOW_COPY_AND_ASSIGN(RepeatedTaskHandle);

  // Start can be used to start a task that will be reposted with a
  // delay determined by the return value of the provided closure.  The returned
  // RepeatedTaskHandle can be moved to the Stop function above to stop the task
  // from continued repetitions. The actual repeated task is owned by the
  // TaskQueue and will live until it has been stopped or the TaskQueue is
  // destroyed. Note that this means that trying to stop the repeated task after
  // the TaskQueue is destroyed is an error. Make sure that you know the
  // lifetime of the TaskQueue is longer than the returned RepeatedTaskHandle.
  template <class Closure>
  static std::unique_ptr<RepeatedTaskHandle> Start(
      rtc::TaskQueue* task_queue,
      TimeDelta first_delay,
      RepeatingTaskIntervalMode interval_mode,
      Closure&& closure) {
    Timestamp first_run_time = Timestamp::us(rtc::TimeMicros()) + first_delay;
    auto periodic_task =
        absl::make_unique<webrtc_repeated_task_impl::RepeatedTaskImpl<Closure>>(
            task_queue, first_run_time, interval_mode,
            std::forward<Closure>(closure));
    auto* periodic_task_ptr = periodic_task.get();
    if (first_delay.IsZero()) {
      task_queue->PostTask(std::move(periodic_task));
    } else {
      task_queue->PostDelayedTask(std::move(periodic_task), first_delay.ms());
    }
    return std::unique_ptr<RepeatedTaskHandle>(
        new RepeatedTaskHandle(periodic_task_ptr));
  }

  template <class Closure>
  static std::unique_ptr<RepeatedTaskHandle> Start(TimeDelta first_delay,
                                                   Closure&& closure) {
    return Start(rtc::TaskQueue::Current(), first_delay,
                 RepeatingTaskIntervalMode::kExcludingExecution,
                 std::forward<Closure>(closure));
  }

  template <class Closure>
  static std::unique_ptr<RepeatedTaskHandle> Start(Closure&& closure) {
    return Start(rtc::TaskQueue::Current(), TimeDelta::Zero(),
                 RepeatingTaskIntervalMode::kExcludingExecution,
                 std::forward<Closure>(closure));
  }
  // Use this function to issue a Stop of future executions of the repeated
  // task. Note that the handle must be moved into the function. This ensures
  // that the same task isn't stopped twice.
  static void Stop(std::unique_ptr<RepeatedTaskHandle> handle);
  // Use this function to issue a Stop of future executions of the repeated
  // task. Note that this version of Stop will only post a Stop, so the repeated
  // task might be running when it returns.
  static void PostStop(std::unique_ptr<RepeatedTaskHandle> handle);

 private:
  explicit RepeatedTaskHandle(
      webrtc_repeated_task_impl::RepeatedTaskBase* repeated_task);
  webrtc_repeated_task_impl::RepeatedTaskBase* repeated_task_;
};

}  // namespace webrtc
#endif  // RTC_BASE_TASK_UTILS_REPEATED_TASK_H_
