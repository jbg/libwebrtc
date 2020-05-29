/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_H_
#define VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "call/adaptation/resource.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

class VideoStreamEncoderResource : public Resource {
 public:
  ~VideoStreamEncoderResource() override;

  void Initialize(TaskQueueBase* encoder_queue,
                  TaskQueueBase* resource_adaptation_queue);

  // Registering task queues must be performed as part of initialization.
  void RegisterEncoderTaskQueue(TaskQueueBase* encoder_queue);
  void UnregisterEncoderTaskQueue();
  void RegisterAdaptationTaskQueue(
      TaskQueueBase* resource_adaptation_queue) override;
  void UnregisterAdaptationTaskQueue() override;

  void AddResourceListener(ResourceListener* listener) override;
  void RemoveResourceListener(ResourceListener* listener) override;

  std::string Name() const override;
  absl::optional<ResourceUsageState> UsageState() const override;
  void ClearUsageState() override;

  bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) const override;
  void OnAdaptationApplied(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) override;

 protected:
  VideoStreamEncoderResource(std::string name);

  void OnResourceUsageStateMeasured(ResourceUsageState usage_state);

  rtc::CriticalSection task_queue_lock_;
  TaskQueueBase* encoder_queue_;  // RTC_GUARDED_BY(task_queue_lock_);
  TaskQueueBase*
      resource_adaptation_queue_;  // RTC_GUARDED_BY(task_queue_lock_);

 private:
  const std::string name_;
  absl::optional<ResourceUsageState> usage_state_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  std::vector<ResourceListener*> listeners_
      RTC_GUARDED_BY(resource_adaptation_queue_);
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_H_
