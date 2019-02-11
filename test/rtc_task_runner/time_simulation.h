/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RTC_TASK_RUNNER_TIME_SIMULATION_H_
#define TEST_RTC_TASK_RUNNER_TIME_SIMULATION_H_

#include <deque>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "rtc_base/fake_clock.h"
#include "system_wrappers/include/clock.h"
#include "test/rtc_task_runner/rtc_task_runner_interfaces.h"

namespace webrtc {
namespace sim_time_task_impl {
class SimulatedTimeTaskRunner;
}  // namespace sim_time_task_impl

class TimeSimulation : public RtcTaskRunnerFactory {
 public:
  explicit TimeSimulation(Timestamp start_time,
                          bool override_global_clock = false);
  ~TimeSimulation() override;
  void Wait(TimeDelta duration) override;
  Clock* GetClock() override;

 protected:
  RtcTaskRunnerImplInterface* Create(absl::string_view queue_name,
                                     TaskQueuePriority priority) override;

 private:
  friend class sim_time_task_impl::SimulatedTimeTaskRunner;
  using SimulatedTimeTaskRunner = sim_time_task_impl::SimulatedTimeTaskRunner;
  void AdvanceTime(Timestamp next_time);
  void Unregister(SimulatedTimeTaskRunner* runner);
  void RunUntil(Timestamp target_time);
  Timestamp GetCurrentTime() const { return current_time_; }
  Timestamp current_time_;
  SimulatedClock sim_clock_;
  std::unique_ptr<rtc::ScopedFakeClock> event_log_fake_clock_;
  std::vector<SimulatedTimeTaskRunner*> task_runners_;
};
}  // namespace webrtc

#endif  // TEST_RTC_TASK_RUNNER_TIME_SIMULATION_H_
