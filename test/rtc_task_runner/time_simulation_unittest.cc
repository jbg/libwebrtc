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

#include "absl/memory/memory.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtc_task_runner/rtc_task_runner.h"
#include "test/rtc_task_runner/time_simulation.h"

// NOTE: Since these tests rely on real time behavior, they will be flaky
// if run on heavily loaded systems.
namespace webrtc {
namespace {
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
constexpr Timestamp kStartTime = Timestamp::Seconds<1000>();
}  // namespace

TEST(TimeSimulationTaskRunnerTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(5);
  const TimeDelta kLongInterval = TimeDelta::ms(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;
  TimeSimulation time_simulation(kStartTime, true);
  RtcTaskRunner task_handler(&time_simulation, "TestQueue");
  std::atomic_int counter(0);
  auto handle = task_handler.Repeat([&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Wait long enough to go through the initial phase.
  time_simulation.Wait(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  handle.PostStop();
  // Wait long enough that the task would run at least once more if not
  // stopped.
  time_simulation.Wait(kLongInterval * 2);
  EXPECT_EQ(counter.load(), kShortIntervalCount);
}

TEST(TimeSimulationTaskRunnerTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  TimeSimulation time_simulation(kStartTime, true);
  RtcTaskRunner task_handler(&time_simulation, "TestQueue");
  TaskHandle handle;
  task_handler.PostTask([&] {
    handle = task_handler.Repeat([&] {
      ++counter;
      handle.Stop();
      return TimeDelta::ms(2);
    });
  });
  time_simulation.Wait(TimeDelta::ms(10));
  EXPECT_EQ(counter.load(), 1);
}
TEST(TimeSimulationTaskRunnerTest, Example) {
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
  TimeSimulation time_simulation(kStartTime, true);
  RtcTaskRunner task_handler(&time_simulation, "TestQueue");
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
}
}  // namespace webrtc
