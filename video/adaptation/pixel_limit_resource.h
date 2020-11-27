/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_PIXEL_LIMIT_RESOURCE_H_
#define VIDEO_ADAPTATION_PIXEL_LIMIT_RESOURCE_H_

#include "api/adaptation/resource.h"
#include "api/scoped_refptr.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

// An adaptation resource designed to be used in the TestBed.
//
// Periodically reports "overuse" until the stream is below the specified
// resolution (expressed as pixel count). Used to simulate being CPU limited.
class PixelLimitResource : public Resource {
 public:
  static rtc::scoped_refptr<PixelLimitResource> Create(
      TaskQueueBase* task_queue,
      size_t max_pixels);

  PixelLimitResource(TaskQueueBase* task_queue, size_t max_pixels);
  ~PixelLimitResource() override;

  // TODO(hbos): Implement feedback mechanism for current and/or restricted
  // resolution and do overuse or underuse based on that...

  // Resource implementation.
  std::string Name() const override { return "PixelLimitResource"; }
  void SetResourceListener(ResourceListener* listener) override;

 private:
  TaskQueueBase* const task_queue_;
  const size_t max_pixels_;
  webrtc::ResourceListener* listener_;
  RepeatingTaskHandle repeating_task_;
  rtc::WeakPtrFactory<PixelLimitResource> weak_ptr_factory_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_PIXEL_LIMIT_RESOURCE_H_
