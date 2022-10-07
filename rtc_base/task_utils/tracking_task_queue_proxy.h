/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_TRACKING_TASK_QUEUE_PROXY_H_
#define RTC_BASE_TASK_UTILS_TRACKING_TASK_QUEUE_PROXY_H_

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class TrackingTaskQueueProxy : public TaskQueueBase {
 public:
  using SampleCallback = absl::AnyInvocable<void(TimeDelta)>;

  TrackingTaskQueueProxy(
      Clock* clock,
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> delegate,
      TimeDelta task_tracking_rate,
      TimeDelta latency_tracking_rate,
      SampleCallback post_latency_cb,
      SampleCallback task_duration_cb);
  TrackingTaskQueueProxy(const TrackingTaskQueueProxy&) = delete;
  TrackingTaskQueueProxy(TrackingTaskQueueProxy&&) = default;
  TrackingTaskQueueProxy& operator=(const TrackingTaskQueueProxy&) = delete;
  TrackingTaskQueueProxy& operator=(TrackingTaskQueueProxy&&) = delete;

  void Delete() override;
  void PostTask(absl::AnyInvocable<void() &&> task) override;
  void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                       TimeDelta delay) override;
  void PostDelayedHighPrecisionTask(absl::AnyInvocable<void() &&> task,
                                    TimeDelta delay) override;

 private:
  void TestPostTaskLatency();
  void OnPostTaskLatencyTest(Timestamp start_time);
  absl::optional<Timestamp> TrackingStartTime();
  void RunTask(absl::AnyInvocable<void() &&> task);

  Clock* const clock_;
  const TimeDelta task_tracking_rate_;
  const TimeDelta latency_tracking_rate_;
  SampleCallback post_latency_cb_;
  SampleCallback task_duration_cb_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> delegate_;
  Timestamp next_task_tracking_time_;
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_TRACKING_TASK_QUEUE_PROXY_H_
