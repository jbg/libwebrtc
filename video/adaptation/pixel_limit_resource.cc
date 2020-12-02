/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/pixel_limit_resource.h"

#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

// static
rtc::scoped_refptr<PixelLimitResource> PixelLimitResource::Create(
    TaskQueueBase* task_queue,
    size_t max_pixels) {
  return new rtc::RefCountedObject<PixelLimitResource>(task_queue, max_pixels);
}

PixelLimitResource::PixelLimitResource(TaskQueueBase* task_queue,
                                       size_t max_pixels)
    : task_queue_(task_queue), max_pixels_(max_pixels) {
  RTC_DCHECK_RUN_ON(task_queue_);
  repeating_task_ = RepeatingTaskHandle::Start(task_queue_, [&] {
    printf("Hello world\n");
    RTC_DCHECK(max_pixels_);
    return TimeDelta::Seconds(1);
  });
}

PixelLimitResource::~PixelLimitResource() {
  RTC_DCHECK_RUN_ON(task_queue_);
  repeating_task_.Stop();
}

void PixelLimitResource::SetResourceListener(ResourceListener* listener) {
  RTC_DCHECK_RUN_ON(task_queue_);
  listener_ = listener;
}

}  // namespace webrtc
