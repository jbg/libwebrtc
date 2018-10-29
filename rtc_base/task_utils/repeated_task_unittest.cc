/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <chrono>
#include <thread>

#include "rtc_base/task_utils/repeated_task.h"
#include "test/gtest.h"

// NOTE: Since these tests rely on real time behavior, they will be flaky
// if run on heavily loaded systems.
namespace webrtc {
namespace {
void Sleep(TimeDelta time_delta) {
  std::this_thread::sleep_for(std::chrono::microseconds(time_delta.us()));
}
}  // namespace

TEST(RepeatedTaskTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(1);
  const TimeDelta kLongInterval = TimeDelta::ms(10);
  const int kShortIntervalCount = 4;
  const int kMargin = 2;

  rtc::TaskQueue task_queue("TestQueue");
  std::atomic_int counter(0);
  std::unique_ptr<RepeatedTaskHandle> handle = RepeatedTaskHandle::Start(
      &task_queue, TimeDelta::Zero(),
      RepeatingTaskIntervalMode::kExcludingExecution, [&] {
        if (++counter >= kShortIntervalCount)
          return kLongInterval;
        return kShortInterval;
      });
  // Sleep long enough to go through the initial phase.
  Sleep(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  RepeatedTaskHandle::PostStop(std::move(handle));
  // Sleep long enough that the task would run at least once more if not
  // stopped.
  Sleep(kLongInterval * 2);
  EXPECT_EQ(counter.load(), 4);
}

TEST(RepeatedTaskTest, CompensatesForRunTimeInInclusiveMode) {
  const int kTargetCount = 20;
  const TimeDelta kRepeatInterval = TimeDelta::ms(1);
  const TimeDelta kSleepDuration = TimeDelta::ms(10);
  const int kMargin = 2;
  const int kSleepAtCount = 3;

  rtc::TaskQueue task_queue("TestQueue");
  std::atomic_int counter(0);
  std::unique_ptr<RepeatedTaskHandle> handle = RepeatedTaskHandle::Start(
      &task_queue, TimeDelta::Zero(),
      RepeatingTaskIntervalMode::kIncludingExecution, [&] {
        if (++counter == kSleepAtCount)
          Sleep(kSleepDuration);
        return kRepeatInterval;
      });
  Sleep(kRepeatInterval * kTargetCount);
  // Execution time should not have affected the run count,
  // but we allow some margin to reduce flakiness.
  EXPECT_GE(counter.load(), kTargetCount - kMargin);
}
TEST(RepeatedTaskTest, NoCompensateForRunTimeInExclusiveMode) {
  const int kTargetCount = 20;
  const TimeDelta kRepeatInterval = TimeDelta::ms(1);
  const TimeDelta kSleepDuration = TimeDelta::ms(10);
  const int kReducedCount = kTargetCount - kSleepDuration / kRepeatInterval;
  const int kMargin = 2;
  const int kSleepAtCount = 3;

  rtc::TaskQueue task_queue("TestQueue");
  std::atomic_int counter(0);
  std::unique_ptr<RepeatedTaskHandle> handle = RepeatedTaskHandle::Start(
      &task_queue, TimeDelta::Zero(),
      RepeatingTaskIntervalMode::kExcludingExecution, [&] {
        if (++counter == kSleepAtCount)
          Sleep(kSleepDuration);
        return kRepeatInterval;
      });
  Sleep(kRepeatInterval * kTargetCount);
  // The longer execution time should have reduced the
  // run count proportionally.
  EXPECT_LE(counter.load(), kReducedCount + kMargin);
}
}  // namespace webrtc
