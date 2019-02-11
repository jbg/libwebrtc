/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtc_task_runner/default_task_runner.h"
#include "test/rtc_task_runner/rtc_task_runner.h"

// NOTE: Since these tests rely on real time behavior, they will be flaky
// if run on heavily loaded systems.
namespace webrtc {
namespace {
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;

constexpr TimeDelta kTimeout = TimeDelta::Millis<1000>();
}  // namespace

TEST(DefaultTaskRunnerTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(5);
  const TimeDelta kLongInterval = TimeDelta::ms(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;

  DefaultTaskRunnerFactory factory;
  RtcTaskRunner task_handler(&factory, "TestQueue");
  std::atomic_int counter(0);
  auto handle = task_handler.Repeat([&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Sleep long enough to go through the initial phase.
  factory.Wait(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  handle.PostStop();
  // Sleep long enough that the task would run at least once more if not
  // stopped.
  factory.Wait(kLongInterval * 2);
  EXPECT_EQ(counter.load(), kShortIntervalCount);
}

TEST(DefaultTaskRunnerTest, CompensatesForLongRunTime) {
  const int kTargetCount = 20;
  const int kTargetCountMargin = 2;
  const TimeDelta kRepeatInterval = TimeDelta::ms(2);
  // Sleeping inside the task for longer than the repeat interval once, should
  // be compensated for by repeating the task faster to catch up.
  const TimeDelta kSleepDuration = TimeDelta::ms(20);
  const int kSleepAtCount = 3;

  std::atomic_int counter(0);
  DefaultTaskRunnerFactory factory;
  RtcTaskRunner task_handler(&factory, "TestQueue");
  task_handler.Repeat([&] {
    if (++counter == kSleepAtCount)
      factory.Wait(kSleepDuration);
    return kRepeatInterval;
  });
  factory.Wait(kRepeatInterval * kTargetCount);
  // Execution time should not have affected the run count,
  // but we allow some margin to reduce flakiness.
  EXPECT_GE(counter.load(), kTargetCount - kTargetCountMargin);
}

TEST(DefaultTaskRunnerTest, CompensatesForShortRunTime) {
  std::atomic_int counter(0);
  DefaultTaskRunnerFactory factory;
  RtcTaskRunner task_handler(&factory, "TestQueue");
  task_handler.Repeat([&] {
    ++counter;
    // Sleeping for the 5 ms should be compensated.
    factory.Wait(TimeDelta::ms(5));
    return TimeDelta::ms(10);
  });
  factory.Wait(TimeDelta::ms(15));
  // We expect that the task have been called twice, once directly at Start and
  // once after 10 ms has passed.
  EXPECT_EQ(counter.load(), 2);
}
TEST(DefaultTaskRunnerTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  DefaultTaskRunnerFactory factory;
  RtcTaskRunner task_handler(&factory, "TestQueue");
  TaskHandle handle;
  task_handler.PostTask([&] {
    handle = task_handler.Repeat([&] {
      ++counter;
      handle.Stop();
      return TimeDelta::ms(2);
    });
  });
  factory.Wait(TimeDelta::ms(10));
  EXPECT_EQ(counter.load(), 1);
}

TEST(DefaultTaskRunnerTest, StartPeriodicTask) {
  MockFunction<TimeDelta()> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(TimeDelta::ms(20)))
      .WillOnce(Return(TimeDelta::ms(20)))
      .WillOnce(Invoke([&done] {
        done.Set();
        return kTimeout;
      }));
  DefaultTaskRunnerFactory factory;
  RtcTaskRunner task_handler(&factory, "TestQueue");
  task_handler.Repeat(closure.AsStdFunction());
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(DefaultTaskRunnerTest, Example) {
  class ObjectOnTaskQueue {
   public:
    void DoPeriodicTask() {}
    TimeDelta TimeUntilNextRun() { return TimeDelta::ms(100); }
    void StartPeriodicTask(TaskHandle* handle, RtcTaskRunner* task_handler) {
      *handle = task_handler->Repeat([this] {
        DoPeriodicTask();
        return TimeUntilNextRun();
      });
    }
  };
  DefaultTaskRunnerFactory factory;
  RtcTaskRunner task_handler(&factory, "TestQueue");
  auto object = absl::make_unique<ObjectOnTaskQueue>();
  // Create and start the periodic task.
  TaskHandle handle;
  object->StartPeriodicTask(&handle, &task_handler);
  // Restart the task
  handle.PostStop();
  object->StartPeriodicTask(&handle, &task_handler);
  handle.PostStop();
  struct Destructor {
    void operator()() { object.reset(); }
    std::unique_ptr<ObjectOnTaskQueue> object;
  };
  task_handler.PostTask(Destructor{std::move(object)});
  // Do not wait for the destructor closure in order to create a race between
  // task queue destruction and running the desctructor closure.
}
}  // namespace webrtc
