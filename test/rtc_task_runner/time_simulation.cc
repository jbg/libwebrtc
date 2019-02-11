/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/rtc_task_runner/time_simulation.h"

#include <algorithm>
#include <string>

#include "absl/memory/memory.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
namespace sim_time_task_impl {
struct DelayedTask {
  Timestamp target_time;
  std::unique_ptr<PendingTaskInterface> task;
};

class RepeatingTaskImpl : public RepeatingTaskHandleImplInterface {
 public:
  RepeatingTaskImpl(Timestamp next_time,
                    std::unique_ptr<RepeatingTaskInterface> task)
      : next_time_(next_time), task_(std::move(task)) {}
  virtual ~RepeatingTaskImpl() = default;
  void Stop() override { next_time_ = Timestamp::PlusInfinity(); }
  void PostStop() override { next_time_ = Timestamp::PlusInfinity(); }
  Timestamp next_time_;
  std::unique_ptr<RepeatingTaskInterface> task_;
};

class SimulatedTimeTaskRunner : public RtcTaskRunnerImplInterface {
 public:
  SimulatedTimeTaskRunner(TimeSimulation* handler,
                          absl::string_view queue_name,
                          TaskQueuePriority priority);
  ~SimulatedTimeTaskRunner() override;
  void Invoke(std::unique_ptr<PendingTaskInterface> task) override {
    task->Run();
  }
  void Post(TimeDelta delay,
            std::unique_ptr<PendingTaskInterface> task) override;
  RepeatingTaskHandleImplInterface* Repeat(
      TimeDelta delay,
      std::unique_ptr<RepeatingTaskInterface> task) override;
  void RunTasks();
  void UpdateTimedTasks(Timestamp at_time);
  Timestamp next_task_time() const { return next_task_time_; }
  bool IsCurrent() const override { return checker_.CalledOnValidThread(); }

 private:
  TimeSimulation* handler_;
  std::string name_;
  std::deque<std::unique_ptr<PendingTaskInterface>> pending_tasks_;
  std::deque<RepeatingTaskImpl*> pending_repeating_tasks_;
  std::list<DelayedTask> delayed_tasks_;
  std::list<RepeatingTaskImpl> repeating_tasks_;
  Timestamp next_task_time_ = Timestamp::PlusInfinity();
  rtc::ThreadChecker checker_;
};

SimulatedTimeTaskRunner::SimulatedTimeTaskRunner(TimeSimulation* handler,
                                                 absl::string_view queue_name,
                                                 TaskQueuePriority priority)
    : handler_(handler),
      name_(queue_name),
      next_task_time_(handler->GetCurrentTime()) {}

SimulatedTimeTaskRunner::~SimulatedTimeTaskRunner() {
  handler_->Unregister(this);
}

void SimulatedTimeTaskRunner::Post(TimeDelta delay,
                                   std::unique_ptr<PendingTaskInterface> task) {
  if (delay <= TimeDelta::Zero()) {
    pending_tasks_.push_back(std::move(task));
    next_task_time_ = Timestamp::MinusInfinity();
  } else {
    auto target_time = handler_->GetCurrentTime() + delay;
    delayed_tasks_.push_back({target_time, std::move(task)});
    next_task_time_ = std::min(next_task_time_, target_time);
  }
}

RepeatingTaskHandleImplInterface* SimulatedTimeTaskRunner::Repeat(
    TimeDelta delay,
    std::unique_ptr<RepeatingTaskInterface> task) {
  delay = std::max(TimeDelta::Zero(), delay);
  auto target_time = handler_->GetCurrentTime() + delay;
  next_task_time_ = std::min(next_task_time_, target_time);
  repeating_tasks_.emplace_back(target_time, std::move(task));
  return &repeating_tasks_.back();
}

void SimulatedTimeTaskRunner::RunTasks() {
  while (!pending_tasks_.empty()) {
    pending_tasks_.front()->Run();
    pending_tasks_.pop_front();
  }
  while (!pending_repeating_tasks_.empty()) {
    auto* task_wrapper = pending_repeating_tasks_.front();
    TimeDelta delay = task_wrapper->task_->Run(task_wrapper->next_time_);
    if (task_wrapper->next_time_.IsFinite()) {
      RTC_DCHECK(delay.IsFinite());
      task_wrapper->next_time_ += delay;
    }
    pending_repeating_tasks_.pop_front();
  }
  if (!pending_tasks_.empty() || !pending_repeating_tasks_.empty()) {
    next_task_time_ = Timestamp::MinusInfinity();
  } else {
    next_task_time_ = Timestamp::PlusInfinity();
    for (auto& delayed : delayed_tasks_)
      next_task_time_ = std::min(next_task_time_, delayed.target_time);
    for (auto& repeating : repeating_tasks_)
      next_task_time_ = std::min(next_task_time_, repeating.next_time_);
  }
}

void SimulatedTimeTaskRunner::UpdateTimedTasks(Timestamp at_time) {
  for (auto it = delayed_tasks_.begin(); it != delayed_tasks_.end();) {
    if (it->target_time <= at_time) {
      pending_tasks_.push_back(std::move(it->task));
      it = delayed_tasks_.erase(it);
    } else {
      ++it;
    }
  }

  for (auto& repeating : repeating_tasks_) {
    if (repeating.next_time_ <= at_time) {
      pending_repeating_tasks_.push_back(&repeating);
    }
  }
  repeating_tasks_.remove_if([](const RepeatingTaskImpl& repeating) {
    return repeating.next_time_.IsPlusInfinity();
  });
}

}  // namespace sim_time_task_impl

TimeSimulation::TimeSimulation(Timestamp start_time, bool override_global_clock)
    : current_time_(start_time), sim_clock_(start_time.us()) {
  if (override_global_clock) {
    event_log_fake_clock_ = absl::make_unique<rtc::ScopedFakeClock>();
    event_log_fake_clock_->SetTimeMicros(start_time.us());
  }
}

TimeSimulation::~TimeSimulation() {}

void TimeSimulation::Wait(TimeDelta duration) {
  RunUntil(current_time_ + duration);
}

Clock* TimeSimulation::GetClock() {
  return &sim_clock_;
}

void TimeSimulation::RunUntil(Timestamp target_time) {
  while (current_time_ <= target_time && !task_runners_.empty()) {
    SimulatedTimeTaskRunner* next_runner = task_runners_.front();
    for (auto* runner : task_runners_) {
      if (runner->next_task_time() <= current_time_) {
        next_runner = runner;
        break;
      } else if (runner->next_task_time() < next_runner->next_task_time()) {
        next_runner = runner;
      }
    }
    if (!next_runner)
      break;
    if (next_runner->next_task_time() > target_time)
      break;
    if (next_runner->next_task_time() > current_time_)
      AdvanceTime(next_runner->next_task_time());
    next_runner->UpdateTimedTasks(current_time_);
    next_runner->RunTasks();
  }
  AdvanceTime(target_time);
}

RtcTaskRunnerImplInterface* TimeSimulation::Create(absl::string_view queue_name,
                                                   TaskQueuePriority priority) {
  auto* new_runner = new SimulatedTimeTaskRunner(this, queue_name, priority);
  task_runners_.push_back(new_runner);
  return new_runner;
}

void TimeSimulation::AdvanceTime(Timestamp next_time) {
  TimeDelta delta = next_time - current_time_;
  current_time_ = next_time;
  sim_clock_.AdvanceTimeMicroseconds(delta.us());
  if (event_log_fake_clock_)
    event_log_fake_clock_->SetTimeMicros(current_time_.us());
}

void TimeSimulation::Unregister(SimulatedTimeTaskRunner* runner) {
  std::remove(task_runners_.begin(), task_runners_.end(), runner);
}

// namespace simulated_time_impl
}  // namespace webrtc
