/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TASK_RUNNER_DEFAULT_TASK_RUNNER_H_
#define TEST_TASK_RUNNER_DEFAULT_TASK_RUNNER_H_
#include <memory>
#include <string>

#include "rtc_base/task_queue.h"
#include "test/task_runner/task_runner_interface.h"

namespace webrtc {
class DefaultTaskRunner : public TaskRunnerImplInterface {
 public:
  DefaultTaskRunner(absl::string_view queue_name, TaskQueuePriority priority)
      : task_queue_(std::string(queue_name).c_str(), priority) {}

  void Invoke(std::unique_ptr<PendingTaskInterface> task) override;
  void Post(TimeDelta delay,
            std::unique_ptr<PendingTaskInterface> task) override;
  RepeatingTaskHandleInterface* Start(
      TimeDelta delay,
      std::unique_ptr<RepeatingTaskInterface> task) override;
  bool IsCurrent() const override;

 private:
  rtc::TaskQueue task_queue_;
};

class DefaultTaskRunnerFactory : public TaskRunnerFactory {
 protected:
  TaskRunnerImplInterface* Create(absl::string_view queue_name,
                                  TaskQueuePriority priority) override;
};
}  // namespace webrtc

#endif  // TEST_TASK_RUNNER_DEFAULT_TASK_RUNNER_H_
