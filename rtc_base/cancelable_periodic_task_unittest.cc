/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancelable_periodic_task.h"

#include "rtc_base/event.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"

namespace {

using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::webrtc::SleepMs;

class MockClosure {
 public:
  MOCK_METHOD0(Call, int());
  MOCK_METHOD0(Delete, void());
};

class MoveOnlyClosure {
 public:
  explicit MoveOnlyClosure(MockClosure* mock) : mock_(mock) {}
  MoveOnlyClosure(const MoveOnlyClosure&) = delete;
  MoveOnlyClosure(MoveOnlyClosure&& other) : mock_(other.mock_) {
    other.mock_ = nullptr;
  }
  ~MoveOnlyClosure() {
    if (mock_)
      mock_->Delete();
  }
  int operator()() { return mock_->Call(); }

 private:
  MockClosure* mock_;
};

TEST(CancelablePeriodicTaskTest, CancelDoesNothingOnEmptyObject) {
  rtc::CancelableTaskHandler handler;
  handler.Cancel();
}

TEST(CancelablePeriodicTaskTest, CancelTaskBeforeItRun) {
  rtc::Event done(false, false);
  MockClosure mock;
  EXPECT_CALL(mock, Call).Times(0);
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));

  rtc::TaskQueue task_queue("queue");

  auto task = rtc::CreateCancelablePeriodicTask(MoveOnlyClosure(&mock));
  rtc::CancelableTaskHandler handler = task->CancelationHandler();
  handler.Cancel();
  task_queue.PostDelayedTask(std::move(task), 0);

  EXPECT_TRUE(done.Wait(20));
}

TEST(CancelablePeriodicTaskTest, CancelTaskAfterItRun) {
  rtc::Event done(false, false);
  rtc::Event run(false, false);
  MockClosure mock;
  EXPECT_CALL(mock, Call).WillOnce(Invoke([&run] {
    run.Set();
    return -1;
  }));
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));

  rtc::TaskQueue task_queue("queue");

  auto task = rtc::CreateCancelablePeriodicTask(MoveOnlyClosure(&mock));
  rtc::CancelableTaskHandler handler = task->CancelationHandler();
  task_queue.PostTask(std::move(task));
  EXPECT_TRUE(done.Wait(20));
  EXPECT_TRUE(run.Wait(0));
  handler.Cancel();
}

TEST(CancelablePeriodicTaskTest, CancelTaskUsingCopyHandler) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).Times(0);
  rtc::TaskQueue task_queue("queue");

  auto task = rtc::CreateCancelablePeriodicTask(closure.AsStdFunction());
  rtc::CancelableTaskHandler handler = task->CancelationHandler();
  task_queue.PostDelayedTask(std::move(task), 20);
  rtc::CancelableTaskHandler copy = handler;
  SleepMs(10);
  copy.Cancel();
  SleepMs(20);
}

TEST(CancelablePeriodicTaskTest, CancelNextTaskWhileRunningTask) {
  rtc::Event started(false, false);
  rtc::Event unpause(false, false);

  rtc::TaskQueue task_queue("queue");
  auto task = rtc::CreateCancelablePeriodicTask([&] {
    started.Set();
    EXPECT_TRUE(unpause.Wait(100));
    return 10;
  });
  rtc::CancelableTaskHandler handler = task->CancelationHandler();
  task_queue.PostDelayedTask(std::move(task), 10);
  EXPECT_TRUE(started.Wait(100));

  handler.Cancel();
  unpause.Set();

  started.Reset();
  EXPECT_FALSE(started.Wait(100));
}

TEST(CancelablePeriodicTaskTest, StopTaskQueueBeforeTaskRun) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).Times(0);
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  auto task = rtc::CreateCancelablePeriodicTask(closure.AsStdFunction());
  handler = task->CancelationHandler();
  task_queue.PostDelayedTask(std::move(task), 20);
}

TEST(CancelablePeriodicTaskTest, StartOneTimeTask) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).WillOnce(Return(-1));
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  auto task = rtc::CreateCancelablePeriodicTask(closure.AsStdFunction());
  handler = task->CancelationHandler();
  task_queue.PostTask(std::move(task));
  SleepMs(50);
}

TEST(CancelablePeriodicTaskTest, ZeroReturnValueRePostsTheTask) {
  MockFunction<int()> closure;
  rtc::Event done(false, false);
  EXPECT_CALL(closure, Call()).WillOnce(Return(0)).WillOnce(Invoke([&done] {
    done.Set();
    return -1;
  }));
  rtc::TaskQueue task_queue("queue");
  auto task = rtc::CreateCancelablePeriodicTask(closure.AsStdFunction());
  task_queue.PostTask(std::move(task));
  EXPECT_TRUE(done.Wait(100));
}

TEST(CancelablePeriodicTaskTest, StartPeriodicTask) {
  MockFunction<int()> closure;
  rtc::Event done(false, false);
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(20))
      .WillOnce(Return(20))
      .WillOnce(Invoke([&done] {
        done.Set();
        return -1;
      }));
  rtc::TaskQueue task_queue("queue");
  task_queue.PostTask(
      rtc::CreateCancelablePeriodicTask(closure.AsStdFunction()));
  EXPECT_TRUE(done.Wait(100));
}

TEST(CancelablePeriodicTaskTest, DeletingHandlerDoesnStopTheTask) {
  rtc::Event run(false, false);
  rtc::TaskQueue task_queue("queue");
  auto task = rtc::CreateCancelablePeriodicTask(([&] {
    run.Set();
    return 0;
  }));
  rtc::CancelableTaskHandler handler = task->CancelationHandler();
  task_queue.PostTask(std::move(task));
  // delete the handler.
  handler = {};
  EXPECT_TRUE(run.Wait(100));
}

}  // namespace
