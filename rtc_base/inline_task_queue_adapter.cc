/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/inline_task_queue_adapter.h"

#include <memory>
#include <utility>

#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

InlineTaskQueueAdapter::InlineTaskQueueAdapter(
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue)
    : base_task_queue_(std::move(base_task_queue)) {}

void InlineTaskQueueAdapter::Delete() {
  base_task_queue_->Delete();
}

void InlineTaskQueueAdapter::PostTask(std::unique_ptr<QueuedTask> task) {
  base_task_queue_->PostTask(std::make_unique<WrapperTask>(
      std::move(task), shared_, this, /*take_queue_slot=*/true));
}

void InlineTaskQueueAdapter::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                             uint32_t millis) {
  base_task_queue_->PostDelayedTask(
      std::make_unique<WrapperTask>(std::move(task), shared_, this,
                                    /*take_queue_slot=*/true),
      millis);
}

void InlineTaskQueueAdapter::PostDelayedHighPrecisionTask(
    std::unique_ptr<QueuedTask> task,
    uint32_t millis) {
  base_task_queue_->PostDelayedHighPrecisionTask(
      std::make_unique<WrapperTask>(std::move(task), shared_, this,
                                    /*take_queue_slot=*/true),
      millis);
}

bool InlineTaskQueueAdapter::SharedState::TryBeginExecutionInline(
    TaskQueueBase* queue,
    TaskQueueBase** original_current_queue)
    RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
  if (queue_size.fetch_add(1, std::memory_order_acquire) != 0)
    return false;
  mu.Lock();
  *original_current_queue = TaskQueueBase::Current();
  SetCurrent(queue);
  return true;
}

void InlineTaskQueueAdapter::SharedState::EndInlineExecution(
    TaskQueueBase* original_current_queue) RTC_UNLOCK_FUNCTION() {
  SetCurrent(original_current_queue);
  mu.Unlock();
  queue_size.fetch_sub(1, std::memory_order_release);
}

void InlineTaskQueueAdapter::SharedState::BeginExecution(bool take_queue_slot)
    RTC_EXCLUSIVE_LOCK_FUNCTION() {
  queue_size.fetch_add(take_queue_slot, std::memory_order_acquire);
  mu.Lock();
}

void InlineTaskQueueAdapter::SharedState::EndExecution() RTC_UNLOCK_FUNCTION() {
  mu.Unlock();
  queue_size.fetch_sub(1, std::memory_order_release);
}

InlineTaskQueueAdapter::WrapperTask::WrapperTask(
    std::unique_ptr<QueuedTask> task,
    rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared,
    TaskQueueBase* queue,
    bool take_queue_slot)
    : task_(std::move(task)),
      shared_(std::move(shared)),
      queue_(queue),
      take_queue_slot_(take_queue_slot) {}

bool InlineTaskQueueAdapter::WrapperTask::Run() {
  CurrentTaskQueueSetter setter(queue_);
  shared_->BeginExecution(take_queue_slot_);
  if (!task_->Run())
    (void)task_.release();
  shared_->EndExecution();
  return true;
}

}  // namespace webrtc
