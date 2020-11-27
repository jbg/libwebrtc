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

namespace webrtc {

// static
rtc::scoped_refptr<PixelLimitResource> PixelLimitResource::Create(
    TaskQueueBase* task_queue,
    size_t max_pixels) {
  return new rtc::RefCountedObject<PixelLimitResource>(task_queue, max_pixels);
}

PixelLimitResource::PixelLimitResource(TaskQueueBase* task_queue,
                                       size_t max_pixels)
    : task_queue_(task_queue),
      max_pixels_(max_pixels),
      weak_ptr_factory_(this) {
  RTC_DCHECK_RUN_ON(task_queue_);
  // TODO(hbos): Maybe we don't event need a weak ptr if we just stop the
  // handle.
  repeating_task_ = RepeatingTaskHandle::Start(
      task_queue_, [weak_this = weak_ptr_factory_.GetWeakPtr()] {
        if (!weak_this)
          return TimeDelta::Seconds(1);
        printf("Hello world\n");
        RTC_DCHECK(weak_this->max_pixels_);
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
