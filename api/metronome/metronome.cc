/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/metronome/metronome.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/queued_task.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

Metronome::TickHandle::TickHandle(TaskQueueBase* task_queue,
                                  std::unique_ptr<QueuedTask> task)
    : task_queue_(task_queue), task_(std::move(task)) {
  RTC_DCHECK(task_queue);
  RTC_DCHECK(task_);
}

void Metronome::TickHandle::Run() {
  if (IsNull()) {
    return;
  }
  rtc::scoped_refptr<Metronome::TickHandle> handle_ref = this;
  task_queue_->PostTask(
      ToQueuedTask([ref = std::move(handle_ref)] { ref->task_->Run(); }));
}

bool Metronome::TickHandle::IsNull() const {
  RTC_DCHECK_EQ(!!task_queue_, !!task_);
  return !task_queue_ && !task_;
}

void Metronome::TickHandle::Stop() {
  task_queue_ = nullptr;
  task_ = nullptr;
}

void Metronome::RunTickTasks() {
  MutexLock lock(&mutex_);
  for (auto& handle : tick_handles_) {
    // TODO: Post on the task queue here.
    handle->Run();
  }
}

// Calls task on every metronome tick.
rtc::scoped_refptr<Metronome::TickHandle> Metronome::AddTickListener(
    TaskQueueBase* task_queue,
    std::unique_ptr<QueuedTask> task) {
  RTC_DCHECK(task_queue);
  RTC_DCHECK(task);
  rtc::scoped_refptr<Metronome::TickHandle> handle =
      new TickHandle(task_queue, std::move(task));
  bool do_start = false;
  {
    MutexLock lock(&mutex_);
    do_start = tick_handles_.empty();
    tick_handles_.push_back(handle);
    RTC_LOG(INFO) << "Tick handles length => " << tick_handles_.size();
  }

  if (do_start) {
    Start();
  }

  return handle;
}

void Metronome::RemoveTickListener(
    rtc::scoped_refptr<Metronome::TickHandle> handle) {
  RTC_DCHECK(handle);
  // Ensure it is stopped before removing.
  handle->Stop();
  bool do_stop = false;
  {
    MutexLock lock(&mutex_);
    auto it = absl::c_find(tick_handles_, std::move(handle));
    if (it != tick_handles_.end()) {
      tick_handles_.erase(it);
    }
    do_stop = tick_handles_.empty();
    RTC_LOG(INFO) << "Tick handles length => " << tick_handles_.size();
  }
  if (do_stop) {
    Stop();
  }
}

}  // namespace webrtc
