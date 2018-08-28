/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CANCELABLE_PERIODIC_TASK_H_
#define RTC_BASE_CANCELABLE_PERIODIC_TASK_H_

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/cancelable_task_handler.h"

namespace rtc {

// CancelablePeriodicTask runs a closure multiple times with delay decided
// by the return value of the closure itself.
// The task can be canceled with the handler returned by CancelationHandler().
// Note that the task can only be canceled on the task queue where it runs.
template <typename Closure>
class CancelablePeriodicTask final : public BaseCancelableTask {
 public:
  // |closure| should return time in ms until next run or negative number if
  // task shouldn't run again.
  explicit CancelablePeriodicTask(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  CancelablePeriodicTask(const CancelablePeriodicTask&) = delete;
  CancelablePeriodicTask& operator=(const CancelablePeriodicTask&) = delete;
  ~CancelablePeriodicTask() override = default;

 private:
  bool Run() override {
    if (BaseCancelableTask::Canceled())
      return true;
    int delay_ms = closure_();
    if (delay_ms < 0)
      return true;
    // Reschedule.
    if (delay_ms == 0)
      TaskQueue::Current()->PostTask(absl::WrapUnique(this));
    else
      TaskQueue::Current()->PostDelayedTask(absl::WrapUnique(this), delay_ms);
    return false;
  }

  Closure closure_;
};

template <typename Closure>
std::unique_ptr<BaseCancelableTask> CreateCancelablePeriodicTask(
    Closure&& closure) {
  using CleanedClosure = typename std::remove_cv<
      typename std::remove_reference<Closure>::type>::type;
  return absl::make_unique<CancelablePeriodicTask<CleanedClosure>>(
      std::forward<CleanedClosure>(closure));
}

}  // namespace rtc

#endif  // RTC_BASE_CANCELABLE_PERIODIC_TASK_H_
