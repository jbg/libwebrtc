/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/tracking_task_queue_proxy.h"

#include <memory>
#include <utility>

namespace webrtc {

TrackingTaskQueueProxy::TrackingTaskQueueProxy(
    Clock* clock,
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> delegate,
    TimeDelta task_tracking_rate,
    TimeDelta latency_tracking_rate,
    SampleCallback post_latency_cb,
    SampleCallback task_duration_cb)
    : clock_(clock),
      task_tracking_rate_(task_tracking_rate),
      latency_tracking_rate_(latency_tracking_rate),
      post_latency_cb_(std::move(post_latency_cb)),
      task_duration_cb_(std::move(task_duration_cb)),
      delegate_(std::move(delegate)),
      next_task_tracking_time_(clock_->CurrentTime() + task_tracking_rate) {
  RTC_DCHECK(delegate_);
  RTC_DCHECK_GT(latency_tracking_rate_, TimeDelta::Zero());

  if (post_latency_cb_) {
    delegate_->PostDelayedTask([this] { TestPostTaskLatency(); },
                               task_tracking_rate_);
  }
}

void TrackingTaskQueueProxy::Delete() {
  delegate_ = nullptr;
}

void TrackingTaskQueueProxy::PostTask(absl::AnyInvocable<void() &&> task) {
  delegate_->PostTask(
      [this, task = std::move(task)]() mutable { RunTask(std::move(task)); });
}

void TrackingTaskQueueProxy::PostDelayedTask(absl::AnyInvocable<void() &&> task,
                                             TimeDelta delay) {
  delegate_->PostDelayedTask(
      [this, task = std::move(task)]() mutable { RunTask(std::move(task)); },
      delay);
}

void TrackingTaskQueueProxy::PostDelayedHighPrecisionTask(
    absl::AnyInvocable<void() &&> task,
    TimeDelta delay) {
  delegate_->PostDelayedHighPrecisionTask(
      [this, task = std::move(task)]() mutable { RunTask(std::move(task)); },
      delay);
}

void TrackingTaskQueueProxy::TestPostTaskLatency() {
  Timestamp start_time = clock_->CurrentTime();
  delegate_->PostTask(
      [this, start_time] { OnPostTaskLatencyTest(start_time); });
}

void TrackingTaskQueueProxy::OnPostTaskLatencyTest(Timestamp start_time) {
  TimeDelta post_task_latency = clock_->CurrentTime() - start_time;
  post_latency_cb_(post_task_latency);
  delegate_->PostDelayedTask([this] { TestPostTaskLatency(); },
                             task_tracking_rate_);
}

absl::optional<Timestamp> TrackingTaskQueueProxy::TrackingStartTime() {
  if (!task_duration_cb_) {
    return absl::nullopt;
  }
  auto now = clock_->CurrentTime();
  if (now >= next_task_tracking_time_) {
    next_task_tracking_time_ = now + task_tracking_rate_;
    return now;
  }
  return absl::nullopt;
}

void TrackingTaskQueueProxy::RunTask(absl::AnyInvocable<void() &&> task) {
  absl::optional<Timestamp> start_time = TrackingStartTime();

  CurrentTaskQueueSetter task_setter(this);
  std::move(task)();

  if (start_time && task_duration_cb_) {
    TimeDelta duration = clock_->CurrentTime() - *start_time;
    task_duration_cb_(duration);
  }
}

}  // namespace webrtc
