/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue_win.h"

// clang-format off
// clang formating would change include order.

// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h. To include win32.h directly, it must be broken out into its own
// build target.
#include <winsock2.h>
#include <windows.h>
#include <sal.h>       // Must come after windows headers.
#include <mmsystem.h>  // Must come after windows headers.
// clang-format on
#include <string.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

void CALLBACK InitializeQueueThread(ULONG_PTR param) {
  rtc::Event* data = reinterpret_cast<rtc::Event*>(param);
  data->Set();
}

rtc::ThreadPriority TaskQueuePriorityToThreadPriority(
    TaskQueueFactory::Priority priority) {
  switch (priority) {
    case TaskQueueFactory::Priority::HIGH:
      return rtc::kRealtimePriority;
    case TaskQueueFactory::Priority::LOW:
      return rtc::kLowPriority;
    case TaskQueueFactory::Priority::NORMAL:
      return rtc::kNormalPriority;
    default:
      RTC_NOTREACHED();
      break;
  }
  return rtc::kNormalPriority;
}

int64_t GetTick() {
  static const UINT kPeriod = 1;
  bool high_res = (timeBeginPeriod(kPeriod) == TIMERR_NOERROR);
  int64_t ret = rtc::TimeMillis();
  if (high_res)
    timeEndPeriod(kPeriod);
  return ret;
}

class DelayedTaskInfo {
 public:
  // Default ctor needed to support priority_queue::pop().
  DelayedTaskInfo() {}
  DelayedTaskInfo(uint32_t milliseconds, std::unique_ptr<QueuedTask> task)
      : due_time_(GetTick() + milliseconds), task_(std::move(task)) {}
  DelayedTaskInfo(DelayedTaskInfo&&) = default;

  // Implement for priority_queue.
  bool operator>(const DelayedTaskInfo& other) const {
    return due_time_ > other.due_time_;
  }

  // Required by priority_queue::pop().
  DelayedTaskInfo& operator=(DelayedTaskInfo&& other) = default;

  // See below for why this method is const.
  std::unique_ptr<QueuedTask> take() const { return std::move(task_); }

  int64_t due_time() const { return due_time_; }

 private:
  int64_t due_time_ = 0;  // Absolute timestamp in milliseconds.

  // |task| needs to be mutable because std::priority_queue::top() returns
  // a const reference and a key in an ordered queue must not be changed.
  // There are two basic workarounds, one using const_cast, which would also
  // make the key (|due_time|), non-const and the other is to make the non-key
  // (|task|), mutable.
  // Because of this, the |task| variable is made private and can only be
  // mutated by calling the |take()| method.
  mutable std::unique_ptr<QueuedTask> task_;
};

class TaskQueueWin : public TaskQueueBase {
 public:
  TaskQueueWin(absl::string_view queue_name, rtc::ThreadPriority priority);
  ~TaskQueueWin() override = default;

  void Delete() override;
  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

  void RunPendingTasks();

 private:
  static void ThreadMain(void* context);

  class WorkerThread : public rtc::PlatformThread {
   public:
    WorkerThread(rtc::ThreadRunFunction func,
                 void* obj,
                 absl::string_view thread_name,
                 rtc::ThreadPriority priority)
        : PlatformThread(func, obj, thread_name, priority) {}

    bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data) {
      return rtc::PlatformThread::QueueAPC(apc_function, data);
    }
  };

  void RunThreadMain();
  void RunDueTasks();
  void ScheduleNextTimer();

  // Since priority_queue<> by defult orders items in terms of
  // largest->smallest, using std::less<>, and we want smallest->largest,
  // we would like to use std::greater<> here. Alas it's only available in
  // C++14 and later, so we roll our own compare template that that relies on
  // operator<().
  template <typename T>
  struct greater {
    bool operator()(const T& l, const T& r) { return l > r; }
  };

  rtc::CriticalSection timer_lock_;
  std::priority_queue<DelayedTaskInfo,
                      std::vector<DelayedTaskInfo>,
                      greater<DelayedTaskInfo>>
      timer_tasks_ RTC_GUARDED_BY(timer_lock_);

  WorkerThread thread_;
  rtc::CriticalSection pending_lock_;
  std::queue<std::unique_ptr<QueuedTask>> pending_
      RTC_GUARDED_BY(pending_lock_);
  // event handle for currently queued tasks
  HANDLE in_queue_;
  // event handle to stop execution
  HANDLE stop_queue_;
  // event handle for waitable timer events
  HANDLE task_timer_;
};

TaskQueueWin::TaskQueueWin(absl::string_view queue_name,
                           rtc::ThreadPriority priority)
    : thread_(&TaskQueueWin::ThreadMain, this, queue_name, priority),
      in_queue_(::CreateEvent(nullptr, TRUE, FALSE, nullptr)),
      stop_queue_(::CreateEvent(nullptr, TRUE, FALSE, nullptr)),
      task_timer_(::CreateWaitableTimer(nullptr, FALSE, nullptr)) {
  RTC_DCHECK(in_queue_);
  RTC_DCHECK(stop_queue_);
  RTC_DCHECK(task_timer_);
  thread_.Start();
  rtc::Event event(false, false);
  RTC_CHECK(thread_.QueueAPC(&InitializeQueueThread,
                             reinterpret_cast<ULONG_PTR>(&event)));
  event.Wait(rtc::Event::kForever);
}

void TaskQueueWin::Delete() {
  RTC_DCHECK(!IsCurrent());
  ::SetEvent(stop_queue_);
  thread_.Stop();
  ::CloseHandle(in_queue_);
  ::CloseHandle(stop_queue_);
  ::CloseHandle(task_timer_);
  delete this;
}

void TaskQueueWin::PostTask(std::unique_ptr<QueuedTask> task) {
  rtc::CritScope lock(&pending_lock_);
  pending_.push(std::move(task));
  ::SetEvent(in_queue_);
}

void TaskQueueWin::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                   uint32_t milliseconds) {
  if (!milliseconds) {
    PostTask(std::move(task));
    return;
  }

  bool need_to_schedule_timer = false;

  {
    rtc::CritScope lock(&timer_lock_);
    // Record if we need to schedule our timer based on if either there are no
    // tasks currently being timed or the current task being timed would end
    // later than the task we're placing into the priority queue
    need_to_schedule_timer =
        timer_tasks_.empty() ||
        timer_tasks_.top().due_time() > GetTick() + milliseconds;

    timer_tasks_.emplace(milliseconds, std::move(task));
  }

  if (need_to_schedule_timer)
    ScheduleNextTimer();
}

void TaskQueueWin::RunPendingTasks() {
  while (true) {
    std::unique_ptr<QueuedTask> task;
    {
      rtc::CritScope lock(&pending_lock_);
      if (pending_.empty())
        break;
      task = std::move(pending_.front());
      pending_.pop();
    }

    if (!task->Run())
      task.release();
  }
}

// static
void TaskQueueWin::ThreadMain(void* context) {
  static_cast<TaskQueueWin*>(context)->RunThreadMain();
}

void TaskQueueWin::RunThreadMain() {
  CurrentTaskQueueSetter set_current(this);
  // These handles correspond to the WAIT_OBJECT_0 + n expressions below where n
  // is the position in handles that WaitForMultipleObjectsEx() was alerted on.
  HANDLE handles[3] = {stop_queue_, task_timer_, in_queue_};
  while (true) {
    // Make sure we do an alertable wait as that's required to allow APCs to
    // run (e.g. required for InitializeQueueThread and stopping the thread in
    // PlatformThread).
    DWORD result = ::WaitForMultipleObjectsEx(arraysize(handles), handles,
                                              FALSE, INFINITE, TRUE);
    RTC_CHECK_NE(WAIT_FAILED, result);

    // run waitable timer tasks
    bool timer_tasks_empty = false;
    {
      rtc::CritScope lock(&timer_lock_);
      timer_tasks_empty = timer_tasks_.empty();
    }
    if (result == WAIT_OBJECT_0 + 1 ||
        (!timer_tasks_empty &&
         ::WaitForSingleObject(task_timer_, 0) == WAIT_OBJECT_0)) {
      RunDueTasks();
      ScheduleNextTimer();
    }

    // reset in_queue_ event and run leftover tasks
    if (result == (WAIT_OBJECT_0 + 2)) {
      ::ResetEvent(in_queue_);
      RunPendingTasks();
    }

    // empty queue and shut down thread
    if (result == (WAIT_OBJECT_0))
      break;
  }
}

void TaskQueueWin::RunDueTasks() {
  rtc::CritScope lock(&timer_lock_);
  RTC_DCHECK(!timer_tasks_.empty());
  auto now = GetTick();
  do {
    if (timer_tasks_.top().due_time() > now)
      break;
    std::unique_ptr<QueuedTask> task = timer_tasks_.top().take();
    timer_tasks_.pop();
    if (!task->Run())
      task.release();
  } while (!timer_tasks_.empty());
}

void TaskQueueWin::ScheduleNextTimer() {
  rtc::CritScope lock(&timer_lock_);
  RTC_DCHECK_NE(task_timer_, INVALID_HANDLE_VALUE);
  if (timer_tasks_.empty())
    return;

  LARGE_INTEGER due_time;
  // We have to convert DelayedTaskInfo's due_time_ from milliseconds to
  // nanoseconds and make it a relative time instead of an absolute time so we
  // multiply it by -10,000 here
  due_time.QuadPart =
      -10000 * std::max(0ll, timer_tasks_.top().due_time() - GetTick());
  ::SetWaitableTimer(task_timer_, &due_time, 0, nullptr, nullptr, FALSE);
}

class TaskQueueWinFactory : public TaskQueueFactory {
 public:
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
        new TaskQueueWin(name, TaskQueuePriorityToThreadPriority(priority)));
  }
};

}  // namespace

std::unique_ptr<TaskQueueFactory> CreateTaskQueueWinFactory() {
  return std::make_unique<TaskQueueWinFactory>();
}

}  // namespace webrtc
