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

#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/task_queue_test.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

TEST(InlineTaskQueueAdapterTest, InlineExecutesWhileNotContended) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));

  auto* thread = rtc::Thread::Current();
  bool called = false;
  adapter.PostTask([&] {
    EXPECT_EQ(rtc::Thread::Current(), thread);
    called = true;
  });
  EXPECT_TRUE(called);
}

TEST(InlineTaskQueueAdapterTest,
     InlineExecutesWhileNotContendedWithTaskQueueBaseInterface) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  TaskQueueBase& task_queue = adapter;
  auto* thread = rtc::Thread::Current();
  bool called = false;
  task_queue.PostTask([&] {
    EXPECT_EQ(rtc::Thread::Current(), thread);
    called = true;
  });
  EXPECT_TRUE(called);
}

TEST(InlineTaskQueueAdapterTest, ExecutesRecursiveTasks) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  rtc::Event event;
  bool called = false;
  adapter.PostTask([&] {
    adapter.PostTask([&] {
      called = true;
      event.Set();
    });
  });
  event.Wait(rtc::Event::kForever);
  EXPECT_TRUE(called);
}

TEST(InlineTaskQueueAdapterTest, ExecutesRecursiveDelayedTasks) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  rtc::Event event;
  bool called = false;
  adapter.PostTask([&] {
    adapter.PostDelayedTask(
        [&] {
          called = true;
          event.Set();
        },
        TimeDelta::Millis(10));
  });
  event.Wait(rtc::Event::kForever);
  EXPECT_TRUE(called);
}

TEST(InlineTaskQueueAdapterTest,
     ExecutesRecursiveDelayedTasksWithTaskQueueInterface) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  TaskQueueBase& task_queue = adapter;
  rtc::Event event;
  bool called = false;
  task_queue.PostTask([&] {
    adapter.PostDelayedTask(
        [&] {
          called = true;
          event.Set();
        },
        TimeDelta::Millis(10));
  });
  event.Wait(rtc::Event::kForever);
  EXPECT_TRUE(called);
}

TEST(InlineTaskQueueAdapterTest, ExecutesRecursiveTasksWithTaskQueueInterface) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  TaskQueueBase& task_queue = adapter;
  rtc::Event event;
  bool called = false;
  task_queue.PostTask([&] {
    task_queue.PostTask([&] {
      called = true;
      event.Set();
    });
  });
  event.Wait(rtc::Event::kForever);
  EXPECT_TRUE(called);
}

TEST(InlineTaskQueueAdapterTest, ExecutesConcurrentTasks) {
  auto factory = CreateDefaultTaskQueueFactory();
  InlineTaskQueueAdapter adapter(
      factory->CreateTaskQueue("test", TaskQueueFactory::Priority::NORMAL));
  auto other_task_queue =
      factory->CreateTaskQueue("other", TaskQueueFactory::Priority::NORMAL);

  // Spawn two concurrent tasks. One that simply executes a task on the adapter,
  // and the other task (here on the main thread) executes two tasks on the same
  // adapter here on the main thread. The tasks posted on the main thread should
  // run in order.
  rtc::Event complete1;
  rtc::Event complete2;
  other_task_queue->PostTask(
      [&] { adapter.PostTask([&] { complete1.Set(); }); });
  int sequence = 0;
  adapter.PostTask([&] { EXPECT_EQ(sequence++, 0); });
  adapter.PostTask([&] {
    EXPECT_EQ(sequence, 1);
    complete2.Set();
  });
  complete1.Wait(rtc::Event::kForever);
  complete2.Wait(rtc::Event::kForever);
}

TEST(InlineTaskQueueAdapterTest,
     ExecutesConcurrentTasksWithTaskQueueInterface) {
  auto factory = CreateDefaultTaskQueueFactory();
  InlineTaskQueueAdapter adapter(
      factory->CreateTaskQueue("test", TaskQueueFactory::Priority::NORMAL));
  auto other_task_queue =
      factory->CreateTaskQueue("other", TaskQueueFactory::Priority::NORMAL);
  TaskQueueBase& adapter_task_queue = adapter;

  // Spawn two concurrent tasks. One that simply executes a task on the adapter,
  // and the other task (here on the main thread) executes two tasks on the same
  // adapter here on the main thread. The tasks posted on the main thread should
  // run in order.
  rtc::Event complete1;
  rtc::Event complete2;
  other_task_queue->PostTask(
      [&] { adapter_task_queue.PostTask([&] { complete1.Set(); }); });
  int sequence = 0;
  adapter_task_queue.PostTask([&] { EXPECT_EQ(sequence++, 0); });
  adapter_task_queue.PostTask([&] {
    EXPECT_EQ(sequence, 1);
    complete2.Set();
  });
  complete1.Wait(rtc::Event::kForever);
  complete2.Wait(rtc::Event::kForever);
}

TEST(InlineTaskQueueAdapterTest, InlineExecutesDuringQueuedDelayedTask) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  rtc::Event complete;
  adapter.PostDelayedTask([&] { complete.Set(); }, TimeDelta::Millis(200));
  bool called = false;
  auto* thread = rtc::Thread::Current();
  adapter.PostTask([&] {
    EXPECT_EQ(rtc::Thread::Current(), thread);
    called = true;
  });
  EXPECT_TRUE(called);
  complete.Wait(rtc::Event::kForever);
}

TEST(InlineTaskQueueAdapterTest,
     InlineExecutesDuringQueuedDelayedTaskWithTaskQueueInterface) {
  InlineTaskQueueAdapter adapter(
      CreateDefaultTaskQueueFactory()->CreateTaskQueue(
          "test", TaskQueueFactory::Priority::NORMAL));
  TaskQueueBase& adapter_task_queue = adapter;
  rtc::Event complete;
  adapter_task_queue.PostDelayedTask([&] { complete.Set(); },
                                     TimeDelta::Millis(200));
  bool called = false;
  auto* thread = rtc::Thread::Current();
  adapter_task_queue.PostTask([&] {
    EXPECT_EQ(rtc::Thread::Current(), thread);
    called = true;
  });
  EXPECT_TRUE(called);
  complete.Wait(rtc::Event::kForever);
}

class InlineTaskQueueFactory : public TaskQueueFactory {
 public:
  virtual std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const {
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> adapter(
        new InlineTaskQueueAdapter(
            CreateDefaultTaskQueueFactory()->CreateTaskQueue(name, priority)));
    return adapter;
  }
};

std::unique_ptr<TaskQueueFactory> CreateInlineTaskQueueFactory(
    const webrtc::FieldTrialsView*) {
  return std::make_unique<InlineTaskQueueFactory>();
}

INSTANTIATE_TEST_SUITE_P(InlineTaskQueue,
                         TaskQueueTest,
                         testing::Values(CreateInlineTaskQueueFactory));

}  // namespace
}  // namespace webrtc
