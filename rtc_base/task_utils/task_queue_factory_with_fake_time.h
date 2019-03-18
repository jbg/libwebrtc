/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_TASK_QUEUE_FACTORY_WITH_FAKE_TIME_H_
#define RTC_BASE_TASK_UTILS_TASK_QUEUE_FACTORY_WITH_FAKE_TIME_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

class TaskQueueFactoryWithFakeTime : public ClockInterface,
                                     public TaskQueueFactory {
 public:
  TaskQueueFactoryWithFakeTime();
  ~TaskQueueFactoryWithFakeTime() override = default;

  // Advance time and runs all pending tasks.
  void Sleep(TimeDelta time);

  // ClockInterface
  int64_t TimeNanos() const override { return now_ns_; }
  // TaskQueueFactory
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) override;

 private:
  class TaskQueue;
  struct DelayedTask {
    DelayedTask(TaskQueue* task_queue, std::unique_ptr<QueuedTask> task)
        : task_queue(task_queue), task(std::move(task)) {}
    TaskQueue* task_queue;
    std::unique_ptr<QueuedTask> task;
  };

  void AddDelayed(uint32_t delay_ms,
                  TaskQueue* task_queue,
                  std::unique_ptr<QueuedTask> task);
  void DeleteTaskQueue(TaskQueue* task_queue);

  int64_t now_ns_ = 123456789;
  rtc::CriticalSection cs_;
  std::vector<TaskQueue*> created_task_queues_ RTC_GUARDED_BY(cs_);
  // Map from time to run a task.
  std::multimap<int64_t, DelayedTask> delayed_tasks_ RTC_GUARDED_BY(cs_);
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_TASK_QUEUE_FACTORY_WITH_FAKE_TIME_H_
