/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TASK_QUEUE_TO_QUEUED_TASK_H_
#define API_TASK_QUEUE_TO_QUEUED_TASK_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/functional/any_invocable.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/queued_task.h"

namespace webrtc {

inline absl::AnyInvocable<void() &&> ToQueuedTask(
    absl::AnyInvocable<void() &&> closure) {
  return closure;
}

inline absl::AnyInvocable<void() &&> ToQueuedTask(
    rtc::scoped_refptr<PendingTaskSafetyFlag> safety,
    absl::AnyInvocable<void() &&> closure) {
  return [safety = std::move(safety), closure = std::move(closure)]() mutable {
    if (safety->alive()) {
      std::move(closure)();
    }
  };
}

inline absl::AnyInvocable<void() &&> ToQueuedTask(
    const ScopedTaskSafety& safety,
    absl::AnyInvocable<void() &&> closure) {
  return ToQueuedTask(safety.flag(), std::move(closure));
}

inline absl::AnyInvocable<void() &&> ToQueuedTask(
    absl::AnyInvocable<void() &&> closure,
    absl::AnyInvocable<void() &&> cleanup) {
  absl::Cleanup c = std::move(cleanup);
  return [closure = std::move(closure), c = std::move(c)]() mutable {
    std::move(closure)();
  };
}

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TO_QUEUED_TASK_H_
