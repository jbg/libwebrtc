/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
#define RTC_BASE_TASK_UTILS_REPEATING_TASK_H_

#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

class RepeatingTask;

namespace webrtc_repeating_task_impl {
enum class IntervalMode {
  // Interpret the returned delay as inclusive of execution time. This means
  // that extra delay and execution time is compensated for. This is appropriate
  // for timed tasks where it's important to keep a specified update rate.
  kIncludingExecution,
  // Interpret the returned delay as exclusive of execution time. This is
  // appropriate for resource intensive tasks without strict timing
  // requirements.
  kExcludingExecution
};
class RepeatingTaskBase : public rtc::QueuedTask {
 public:
  RepeatingTaskBase(rtc::TaskQueue* task_queue,
                    TimeDelta first_delay,
                    IntervalMode interval_mode);
  ~RepeatingTaskBase() override;
  virtual TimeDelta RunClosure() = 0;

 private:
  friend class ::webrtc::RepeatingTask;

  bool Run() final;
  void Stop() RTC_RUN_ON(task_queue_);
  void PostStop();

  rtc::TaskQueue* const task_queue_;
  const IntervalMode interval_mode_;
  bool running_ RTC_GUARDED_BY(task_queue_) = true;
  Timestamp next_run_time_ RTC_GUARDED_BY(task_queue_);
};

// The template closure pattern is based on rtc::ClosureTask.
template <class Closure>
class RepeatingTaskImpl final : public RepeatingTaskBase {
 public:
  RepeatingTaskImpl(rtc::TaskQueue* task_queue,
                    TimeDelta first_delay,
                    IntervalMode interval_mode,
                    Closure&& closure)
      : RepeatingTaskBase(task_queue, first_delay, interval_mode),
        closure_(std::forward<Closure>(closure)) {
    static_assert(
        std::is_same<TimeDelta,
                     typename std::result_of<decltype (&Closure::operator())(
                         Closure)>::type>::value,
        "");
  }

  TimeDelta RunClosure() override { return closure_(); }

 private:
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};
}  // namespace webrtc_repeating_task_impl

class RepeatingTask {
 public:
  using IntervalMode = webrtc_repeating_task_impl::IntervalMode;
  explicit RepeatingTask(
      IntervalMode interval_mode = IntervalMode::kIncludingExecution);
  RTC_DISALLOW_COPY_AND_ASSIGN(RepeatingTask);
  bool Running() const;

  // Start can be used to start a task that will be reposted with a delay
  // determined by the return value of the provided closure. The actual task is
  // owned by the TaskQueue and will live until it has been stopped or the
  // TaskQueue is destroyed. Note that this means that trying to stop the
  // repeating task after the TaskQueue is destroyed is an error. Make sure that
  // you know the lifetime of the TaskQueue is longer than the returned
  // RepeatingTask.
  template <class Closure>
  void Start(rtc::TaskQueue* task_queue, Closure&& closure) {
    RTC_DCHECK(!repeating_task_);
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    repeating_task_ =
        new webrtc_repeating_task_impl::RepeatingTaskImpl<Closure>(
            task_queue, TimeDelta::Zero(), interval_mode_,
            std::forward<Closure>(closure));
    // Transfers ownership to the task queue.
    task_queue->PostTask(absl::WrapUnique(repeating_task_));
  }
  template <class Closure>
  void Start(Closure&& closure) {
    Start(rtc::TaskQueue::Current(), std::forward<Closure>(closure));
  }

  template <class Closure>
  void DelayStart(rtc::TaskQueue* task_queue,
                  TimeDelta first_delay,
                  Closure&& closure) {
    RTC_DCHECK(!repeating_task_);
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    repeating_task_ =
        new webrtc_repeating_task_impl::RepeatingTaskImpl<Closure>(
            task_queue, first_delay, interval_mode_,
            std::forward<Closure>(closure));
    // Transfers ownership to the task queue.
    task_queue->PostDelayedTask(absl::WrapUnique(repeating_task_),
                                first_delay.ms());
  }
  template <class Closure>
  void DelayStart(TimeDelta first_delay, Closure&& closure) {
    DelayStart(rtc::TaskQueue::Current(), first_delay,
               std::forward<Closure>(closure));
  }

  // Use this function to Stop future executions of the repeating task, this
  // must be called from the task queue where the task is running. Note that the
  // object is reset after this, calling Stop twice is an error.
  void Stop();
  // Use this function to issue a Stop of future executions of the repeating
  // task. Note that this version of Stop will only post a Stop, so the
  // repeating task might be running when it returns.
  void PostStop();

 private:
  const IntervalMode interval_mode_;
  rtc::SequencedTaskChecker sequence_checker_;
  webrtc_repeating_task_impl::RepeatingTaskBase* repeating_task_ = nullptr;
};

}  // namespace webrtc
#endif  // RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
