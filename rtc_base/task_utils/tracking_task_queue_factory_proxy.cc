/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/tracking_task_queue_factory_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/task_utils/tracking_task_queue_proxy.h"

namespace webrtc {

namespace {}  // namespace

TrackingTaskQueueFactoryProxy::TrackingTaskQueueFactoryProxy(
    Clock* clock,
    std::unique_ptr<TaskQueueFactory> factory,
    TimeDelta post_task_latency_sampling_rate,
    TimeDelta task_sampling_rate,
    SampleCallback post_latency_cb,
    SampleCallback task_duration_cb)
    : clock_(clock),
      delegate_(std::move(factory)),
      post_latency_cb_(std::move(post_latency_cb)),
      task_duration_cb_(std::move(task_duration_cb)),
      task_sampling_rate_(task_sampling_rate),
      post_task_latency_sampling_rate_(post_task_latency_sampling_rate) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(delegate_);
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
TrackingTaskQueueFactoryProxy::CreateTaskQueue(absl::string_view name,
                                               Priority priority) const {
  auto delegate_tq = delegate_->CreateTaskQueue(name, priority);
  RTC_DCHECK(delegate_tq);

  std::string name_copy(name);
  TrackingTaskQueueProxy::SampleCallback scoped_latency_cb =
      [this, name_copy](TimeDelta latency) {
        post_latency_cb_(name_copy, latency);
      };

  TrackingTaskQueueProxy::SampleCallback scoped_task_duration_cb =
      [this, name_copy](TimeDelta duration) {
        task_duration_cb_(name_copy, duration);
      };

  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      new TrackingTaskQueueProxy(
          clock_, std::move(delegate_tq), task_sampling_rate_,
          post_task_latency_sampling_rate_, std::move(scoped_latency_cb),
          std::move(scoped_task_duration_cb)));
}

}  // namespace webrtc
