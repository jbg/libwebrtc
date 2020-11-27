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

#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

// Get the task queue from...
// GlobalSimulatedTimeController time_controller_
// ... instead?

TEST(PixelLimitResourceTest, HelloWorld) {
  TaskQueueForTest task_queue("TestQueue");
  rtc::scoped_refptr<PixelLimitResource> pixel_limit_resource;
  task_queue.SendTask(
      [&]() {
        pixel_limit_resource =
            PixelLimitResource::Create(task_queue.Get(), 123);
      },
      RTC_FROM_HERE);

  task_queue.SendTask([&]() { pixel_limit_resource = nullptr; }, RTC_FROM_HERE);
}

}  // namespace webrtc
