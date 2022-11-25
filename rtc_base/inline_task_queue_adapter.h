/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_INLINE_TASK_QUEUE_ADAPTER_H_
#define RTC_BASE_INLINE_TASK_QUEUE_ADAPTER_H_

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/make_ref_counted.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

// Task queue adapter that enables inline execution of tasks at PostTask time.
// Notes:
// 1. Inline execution in this manner requires mutexes to be recursive, in
// the case the same mutex is used on the PostTask and the execution side.
// 2. Since inline execution implies the calling thread executes the task, the
// QoS of the wrapped task queue isn't used during inline execution. This may or
// may not be an issue.
class InlineTaskQueueAdapter : public webrtc::TaskQueueBase {
 public:
  explicit InlineTaskQueueAdapter(
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue);

  void Delete() override;
  void PostTask(absl::AnyInvocable<void() &&> task) override;
  void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                       TimeDelta duration) override;
  void PostDelayedHighPrecisionTask(absl::AnyInvocable<void() &&> task,
                                    TimeDelta duration) override;

  // Inline version of PostTask. This has the advantage that it avoids memory
  // allocation for the passed lambda altogether in simple cases.
  template <class Lambda>
  void PostTask(Lambda&& lambda) {
    if (shared_state_->TryBeginInlineExecution()) {
      CurrentTaskQueueSetter setter(this);
      std::forward<Lambda>(lambda)();
      shared_state_->EndInlineExecution();
    } else {
      // The queue slot was already taken in TryBeginInlineExecution.
      base_task_queue_->PostTask(WrappedImmediateTask(
          std::forward<Lambda>(lambda), shared_state_, this,
          /*queue_slot_taken=*/true));
    }
  }

 private:
  class WrappedImmediateTask;
  class WrappedDelayedTask;
  class RTC_LOCKABLE SharedState {
   public:
    // Attempts to begin inline execution. If this succeeds (no tasks are
    // currently running or queued), the method returns true. Otherwise we
    // return false. A queue slot is taken on success, forcing other callers to
    // do regular PostTasks while inline executing.
    bool TryBeginInlineExecution() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true);
    // Ends execution. Releases a taken queue slot.
    void EndInlineExecution() RTC_UNLOCK_FUNCTION();

   private:
    friend class WrappedImmediateTask;
    friend class WrappedDelayedTask;

    // Indicates how many tasks are queued for execution. Note: This is an
    // atomic to avoid lock order inversion if the task queue is reentered
    // during task execution.
    std::atomic<int> queue_size_{0};
    // Ensures delayed and normal posted tasks run in isolation.
    Mutex task_mu_;
  };

  // Immediate task (i.e. from PostTask) that synchronizes on
  // `SharedState::task_mu_` and holds a queue slot throughout it's lifetime.
  class WrappedImmediateTask {
   public:
    // Creates a wrapper task. The `queue_slot_taken` parameter indicates if
    // we've already increased `SharedState::queue_size_` and is true for failed
    // inline execution attempts.
    WrappedImmediateTask(
        absl::AnyInvocable<void() &&> task,
        rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>>
            shared_state,
        TaskQueueBase* queue,
        bool queue_slot_taken);
    WrappedImmediateTask(WrappedImmediateTask&&) = default;
    ~WrappedImmediateTask();
    void operator()();

   protected:
    absl::AnyInvocable<void() &&> task_;
    rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared_state_;
    TaskQueueBase* const queue_;
  };

  // Delayed task (i.e. from PostTask) that synchronizes on
  // `SharedState::task_mu_`. The class only holds a queue slot while executing,
  // allowing inline execution before the delayed task is executed.
  class WrappedDelayedTask {
   public:
    // Creates a wrapper task. The `queue_slot_taken` parameter indicates if
    // we've already increased `SharedState::queue_size_` and is true for failed
    // inline execution attempts.
    WrappedDelayedTask(
        absl::AnyInvocable<void() &&> task,
        rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>>
            shared_state,
        TaskQueueBase* queue);
    void operator()();

   protected:
    absl::AnyInvocable<void() &&> task_;
    rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared_state_;
    TaskQueueBase* const queue_;
  };

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue_;
  rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared_state_ =
      rtc::make_ref_counted<SharedState>();
};

}  // namespace webrtc
#endif  // RTC_BASE_INLINE_TASK_QUEUE_ADAPTER_H_
