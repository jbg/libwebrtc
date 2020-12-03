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

#include <memory>
#include <utility>

#include "api/units/timestamp.h"
#include "call/adaptation/test/fake_video_stream_input_state_provider.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

namespace {

constexpr TimeDelta kResourceUsageCheckIntervalMs = TimeDelta::Millis(5000);

}  // namespace

class PixelLimitResourceTest : public ::testing::Test {
 public:
  PixelLimitResourceTest()
      : time_controller_(Timestamp::Micros(1234)),
        task_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "TestQueue",
            TaskQueueFactory::Priority::NORMAL)),
        input_state_provider_() {}

  void RunTaskOnTaskQueue(std::unique_ptr<QueuedTask> task) {
    task_queue_->PostTask(std::move(task));
    time_controller_.AdvanceTime(TimeDelta::Millis(0));
  }

 protected:
  // Posted tasks, including repeated tasks, are executed when simulated time is
  // advanced by time_controller_.AdvanceTime().
  GlobalSimulatedTimeController time_controller_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
  FakeVideoStreamInputStateProvider input_state_provider_;
};

TEST_F(PixelLimitResourceTest, ResourceIsSilentByDefault) {
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue(ToQueuedTask([&]() {
    rtc::scoped_refptr<PixelLimitResource> pixel_limit_resource =
        PixelLimitResource::Create(task_queue_.get(), &input_state_provider_);
    pixel_limit_resource->SetResourceListener(&resource_listener);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs);
  }));
}

}  // namespace webrtc
