/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/task_queue_libevent.h"

namespace webrtc {
namespace {
class TaskQueueFactoryWrapper : public TaskQueueFactory {
 public:
  explicit TaskQueueFactoryWrapper(TaskQueueFactory* ptr) : ptr_(ptr) {}

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return ptr_->CreateTaskQueue(name, priority);
  }

 private:
  TaskQueueFactory* ptr_;
};
static TaskQueueFactory* factory_override = nullptr;
}  // namespace
void OverrideDefaultTaskQueueFactory(TaskQueueFactory* factory) {
  factory_override = factory;
}
std::unique_ptr<TaskQueueFactory> CreateDefaultTaskQueueFactory() {
  if (factory_override)
    return std::unique_ptr<TaskQueueFactory>(
        new TaskQueueFactoryWrapper(factory_override));
  return CreateTaskQueueLibeventFactory();
}

}  // namespace webrtc
