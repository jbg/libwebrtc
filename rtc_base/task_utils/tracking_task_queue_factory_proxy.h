/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_TRACKING_TASK_QUEUE_FACTORY_PROXY_H_
#define RTC_BASE_TASK_UTILS_TRACKING_TASK_QUEUE_FACTORY_PROXY_H_

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class TrackingTaskQueueFactoryProxy : public TaskQueueFactory {
 public:
  using SampleCallback = absl::AnyInvocable<void(std::string, TimeDelta)>;

  TrackingTaskQueueFactoryProxy(Clock* clock,
                                std::unique_ptr<TaskQueueFactory> factory,
                                TimeDelta post_task_latency_sampling_rate,
                                TimeDelta task_sampling_rate,
                                SampleCallback post_latency_cb,
                                SampleCallback task_duration_cb);

  ~TrackingTaskQueueFactoryProxy() override = default;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override;

 private:
  void OnPostTaskLatency(absl::string_view name, TimeDelta latency);

  Clock* const clock_;
  std::unique_ptr<TaskQueueFactory> const delegate_;
  mutable SampleCallback post_latency_cb_;
  mutable SampleCallback task_duration_cb_;
  const TimeDelta task_sampling_rate_;
  const TimeDelta post_task_latency_sampling_rate_;
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_TRACKING_TASK_QUEUE_FACTORY_PROXY_H_
