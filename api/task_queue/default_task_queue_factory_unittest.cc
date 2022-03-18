/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/task_queue/default_task_queue_factory.h"

#include "api/task_queue/task_queue_test.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

INSTANTIATE_TEST_SUITE_P(Default,
                         TaskQueueTest,
                         ::testing::Values(CreateDefaultTaskQueueFactory));

class MutexMaster {
 public:
  MutexMaster(TaskQueueBase* task_queue0,
              TaskQueueBase* task_queue1,
              TaskQueueBase* task_queue2,
              TaskQueueBase* task_queue3)
      : num_pending_tasks_(0),
        task_queue0_(task_queue0),
        task_queue1_(task_queue1),
        task_queue2_(task_queue2),
        task_queue3_(task_queue3) {}

  void PostPendingTasks() {
    num_pending_tasks_ += 2;
    RTC_LOG(LS_ERROR) << "num_pending_tasks_ += 2: " << num_pending_tasks_;
    task_queue0_->PostTask(ToQueuedTask(
        [this]() { DoSomethingWithM0AndPostToDoSomethingWithM1(); }));
    task_queue1_->PostTask(ToQueuedTask(
        [this]() { DoSomethingWithM1AndPostToDoSomethingWithM0(); }));
  }

  void WaitUntilPendingTasksIsZero() {
    while (num_pending_tasks_ > 0) {
      // Zzz...
    }
  }

 protected:
  void DoSomethingWithM0AndPostToDoSomethingWithM1() {
    MutexLock lock_m1(&m0_);
    RTC_LOG(LS_ERROR) << "Got m0";
    task_queue2_->PostTask(ToQueuedTask([this]() { DoSomethingWithM1(); }));
  }

  void DoSomethingWithM1AndPostToDoSomethingWithM0() {
    MutexLock lock_m1(&m1_);
    RTC_LOG(LS_ERROR) << "Got m1";
    task_queue3_->PostTask(ToQueuedTask([this]() { DoSomethingWithM0(); }));
  }

  void DoSomethingWithM0() {
    MutexLock lock_m0(&m0_);
    RTC_LOG(LS_ERROR) << "Got m0";
    --num_pending_tasks_;
    RTC_LOG(LS_ERROR) << "--num_pending_tasks_: " << num_pending_tasks_;
  }

  void DoSomethingWithM1() {
    MutexLock lock_m0(&m1_);
    RTC_LOG(LS_ERROR) << "Got m1";
    --num_pending_tasks_;
    RTC_LOG(LS_ERROR) << "--num_pending_tasks_: " << num_pending_tasks_;
  }

 public:
  std::atomic<size_t> num_pending_tasks_;
  TaskQueueBase* task_queue0_;
  TaskQueueBase* task_queue1_;
  TaskQueueBase* task_queue2_;
  TaskQueueBase* task_queue3_;
  Mutex m0_;
  Mutex m1_;
};

TEST(MarkusOwesMeA, FreeBeer) {
  auto task_queue_factory = CreateDefaultTaskQueueFactory();

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue0 =
      task_queue_factory->CreateTaskQueue("task_queue0",
                                          TaskQueueFactory::Priority::NORMAL);
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue1 =
      task_queue_factory->CreateTaskQueue("task_queue1",
                                          TaskQueueFactory::Priority::NORMAL);
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue2 =
      task_queue_factory->CreateTaskQueue("task_queue2",
                                          TaskQueueFactory::Priority::NORMAL);
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue3 =
      task_queue_factory->CreateTaskQueue("task_queue3",
                                          TaskQueueFactory::Priority::NORMAL);

  MutexMaster mutex_master(task_queue0.get(), task_queue1.get(),
                           task_queue2.get(), task_queue3.get());
  for (size_t i = 0; i < 10000; ++i) {
    mutex_master.PostPendingTasks();
  }
  mutex_master.WaitUntilPendingTasksIsZero();
}

}  // namespace
}  // namespace webrtc
