/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TASK_RUNNER_TASK_RUNNER_INTERFACE_H_
#define TEST_TASK_RUNNER_TASK_RUNNER_INTERFACE_H_
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_priority.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
class PendingTaskInterface {
 public:
  virtual ~PendingTaskInterface() = default;
  virtual void Run() = 0;
};
class RepeatingTaskHandleImplInterface {
 public:
  virtual void Stop() = 0;
  virtual void PostStop() = 0;

 protected:
  ~RepeatingTaskHandleImplInterface() = default;
};

class RepeatingTaskInterface {
 public:
  virtual ~RepeatingTaskInterface() = default;
  virtual TimeDelta Run(Timestamp at_time) = 0;
};

class TaskRunnerImplInterface {
 protected:
  friend class TaskRunner;
  virtual ~TaskRunnerImplInterface() = default;
  virtual bool IsCurrent() const = 0;
  virtual void Invoke(std::unique_ptr<PendingTaskInterface> task) = 0;
  virtual void Post(TimeDelta delay,
                    std::unique_ptr<PendingTaskInterface> task) = 0;
  virtual RepeatingTaskHandleImplInterface* Repeat(
      TimeDelta delay,
      std::unique_ptr<RepeatingTaskInterface> task) = 0;
};

class TaskRunnerFactory {
 public:
  virtual ~TaskRunnerFactory() = default;

 protected:
  friend class TaskRunner;
  virtual TaskRunnerImplInterface* Create(absl::string_view queue_name,
                                          TaskQueuePriority priority) = 0;
};
}  // namespace webrtc

#endif  // TEST_TASK_RUNNER_TASK_RUNNER_INTERFACE_H_
