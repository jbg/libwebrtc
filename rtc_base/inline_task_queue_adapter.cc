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
    std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> task_queue)
    : queue_(std::move(task_queue)) {}
InlineTaskQueueAdapter::~InlineTaskQueueAdapter() {
  queue_->Delete();
}

bool InlineTaskQueueAdapter::IsCurrent() const {
  return queue_->IsCurrent();
}

void InlineTaskQueueAdapter::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                             uint32_t millis) {
  queue_->PostDelayedTask(
      std::make_unique<WrapperTask>(std::move(task), shared_), millis);
}

void InlineTaskQueueAdapter::CurrentTaskQueueSetterExposer::SetCurrent(
    TaskQueueBase* queue) {
  TaskQueueBase::SetCurrent(queue);
}

bool InlineTaskQueueAdapter::SharedState::TryBeginExecutionInline(
    TaskQueueBase* queue,
    TaskQueueBase** original_current_queue)
    RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
  if (queue_size.fetch_add(1, std::memory_order_acquire) != 0)
    return false;
  mu.Lock();
  *original_current_queue = TaskQueueBase::Current();
  CurrentTaskQueueSetterExposer::SetCurrent(queue);
  return true;
}

void InlineTaskQueueAdapter::SharedState::EndInlineExecution(
    TaskQueueBase* original_current_queue) RTC_UNLOCK_FUNCTION() {
  CurrentTaskQueueSetterExposer::SetCurrent(original_current_queue);
  mu.Unlock();
  queue_size.fetch_sub(1, std::memory_order_release);
}

void InlineTaskQueueAdapter::SharedState::BeginExecution()
    RTC_EXCLUSIVE_LOCK_FUNCTION() {
  queue_size.fetch_add(1, std::memory_order_acquire);
  mu.Lock();
}

void InlineTaskQueueAdapter::SharedState::EndExecution() RTC_UNLOCK_FUNCTION() {
  mu.Unlock();
  queue_size.fetch_sub(1, std::memory_order_release);
}

InlineTaskQueueAdapter::WrapperTask::WrapperTask(
    std::unique_ptr<QueuedTask> task,
    rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared)
    : task_(std::move(task)), shared_(std::move(shared)) {}
bool InlineTaskQueueAdapter::WrapperTask::Run() {
  shared_->BeginExecution();
  if (!task_->Run())
    (void)task_.release();
  shared_->EndExecution();
  return true;
}

}  // namespace webrtc
