/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/task_queue_factory_with_fake_time.h"

#include <deque>

namespace webrtc {

class TaskQueueFactoryWithFakeTime::TaskQueue : public TaskQueueBase {
 public:
  void PostTask(std::unique_ptr<QueuedTask> task) {
    std::unique_ptr<QueuedTask> task_to_run;
    {
      rtc::CritScopy lock(&cs_);
      tasks_.push_back(task);
      if (running_)
        return;
      running_ = true;
      task_to_run = std::move(tasks_.front());
      tasks_.pop_front();
    }
    RunReadyPostedTasks(std::move(task_to_run));
  }
  void PostTaskWithoutRunning(std::unique_ptr<QueuedTask> task) {
    rtc::CritScopy lock(&cs_);
    tasks_.push_back(task);
  }
  void RunReadyTasks() {
    std::unique_ptr<QueuedTask> task_to_run;
    {
      rtc::CritScopy lock(&cs_);
      if (running_)
        return;
      running_ = true;
      task_to_run = std::move(tasks_.front());
      tasks_.pop_front();
    }
    RunReadyPostedTasks(std::move(task_to_run));
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t delay_ms) {
    delayed_tasks_runner_->AddDelayed(delay_ms, this, std::move(task));
  }

  void Delete() override {
    delayed_tasks_runner_->DeleteTaskQueue(this);
    {
      rtc::CritScope lock(&cs_);
      // unsure how to wait for running task, so assume there is none.
      CHECK(!running_);
    }
    delete this;
  }

 private:
  void RunReadyPostedTasks(std::unique_ptr<QueuedTask> task) {
    SetCurrentTaskQueue set_current(this);
    while (true) {
      if (task->Run()) {
        task.reset();  // i.e. delete the task while 'on task queue'.
      } else {
        task.release();
      }
      // Fetch next.
      rtc::CritScope lock(&cs_);
      if (tasks_.empty()) {
        running_ = false;
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
  }

  TaskQueueFactoryWithFakeTime* const delayed_tasks_runner_;

  rtc::CriticalSection cs_;
  bool running_ GUARDED_BY(cs_) = false;
  std::deque<std::unique_ptr<QueuedTask>> tasks_ GUARDED_BY(cs_);
};

void TaskQueueFactoryWithFakeTime::AddDelayed(
    uint32_t delay_ms,
    TaskQueue* task_queue,
    std::unique_ptr<QueuedTask> task) {
  rtc::CritScope lock(&cs_);
  delayed_tasks_.emplace(now_ns_ + delay_ms * 1000000,
                         {task_queue, std::move(task)});
}

void TaskQueueFactoryWithFakeTime::TaskQueueFactoryWithFakeTime::
    DeleteTaskQueue(TaskQueue* task_queue) {
  std::vector<std::unique_ptr<QueuedTask>> tasks_to_delete;
  {
    rtc::CritScope lock(&cs_);
    CHECK_GT(created_task_queues_.erase(task_queue), 0);
    for (auto it = delayed_tasks_.begin(); it != delayed_tasks_.end();) {
      if (it->second.task_queue == task_queue) {
        tasks_to_delete.push_back(std::move(it->second.task));
        it = delayed_tasks_.erase(it);
      } else {
        ++it;
      }
    }
  }
  // To be safer avoiding deadlocks, delete all pending tasks without holding
  // the critical section. You never know what destructors can do.
}

void TaskQueueFactoryWithFakeTime::Sleep(TimeDelta time) {
  cs_.Lock();
  while (!delayed_tasks_.empty() &&
         now_ns_ + time.ns() > delayed_tasks_.front().first) {
    auto it = delayed_tasks_.begin();
    int64_t run_at_ns = it->first;
    TaskQueue* task_queue = it->second.task_queue;
    auto task = std::move(it->second.task);
    delayed_tasks_.erase(it);
    task_queue->PostTaskWithoutRunning(std::move(task));
    cs_.Unlock();
    int64_t time_unill_run_ns = run_at_ns - now_ns;
    DCHECK_GE(time_unill_run, 0);
    now_ns_ = run_at_ns;
    time -= TimeDelta.ns(time_unill_run_ns);
    // There is actually a race here between Sleep() and DeleteTaskQueue.
    // TODO(danilchap): Restrict usage to avood the race or solve it.
    task_queue->RunReadyTasks();
    // May be should run tasks on all queues.
    cs_.Lock();
  }
  cs_.Unlock();
  now_ns_ += time.ns();
}

}  // namespace webrtc
