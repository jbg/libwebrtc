/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CONNECTION_ENVIRONMENT_BUILDER_H_
#define API_CONNECTION_ENVIRONMENT_BUILDER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/connection_environment.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

class RTC_EXPORT ConnectionEnvironmentBuilder {
 public:
  ConnectionEnvironmentBuilder() = default;
  explicit ConnectionEnvironmentBuilder(const ConnectionEnvironment& env);

  ConnectionEnvironmentBuilder(const ConnectionEnvironmentBuilder&) = default;
  ConnectionEnvironmentBuilder& operator=(const ConnectionEnvironmentBuilder&) =
      default;

  ~ConnectionEnvironmentBuilder() = default;

  ConnectionEnvironment Build();

  // Add an owner object to keep reference to in the built ConnectionEnvironment
  // object. Intendeded usage is to pass owner and raw pointers as separate
  // objects,
  // e.g. .With(&ptr->event_log).With(&ptr->trials).WithStorage(std::move(ptr))
  template <typename T>
  ConnectionEnvironmentBuilder& WithStorage(T value) {
    deleters_.push_back([value = std::move(value)] {});
    return *this;
  }

  // Adds non-owning reference to the built evironment.
  // Does nothing when passed pointer is nullptr.
  template <typename Ptr>
  ConnectionEnvironmentBuilder& With(Ptr* ptr) {
    if (ptr != nullptr) {
      Set(ptr);
    }
    return *this;
  }

  // Adds owning reference to the built evironment.
  // Does nothing when passed pointer is nullptr.
  template <typename Ptr>
  ConnectionEnvironmentBuilder& With(std::unique_ptr<Ptr> ptr) {
    if (ptr != nullptr) {
      Set(ptr.get());
      deleters_.push_back([ptr = std::move(ptr)] {});
    }
    return *this;
  }

 private:
  void Set(Clock* clock) { clock_ = clock; }
  void Set(TaskQueueFactory* task_queue_factory) {
    task_queue_factory_ = task_queue_factory;
  }
  void Set(const FieldTrialsView* experiments) { experiments_ = experiments; }
  void Set(RtcEventLog* event_log) { event_log_ = event_log; }

  rtc::scoped_refptr<rtc::RefCountedBase> storage_;
  std::vector<absl::AnyInvocable<void() &&>> deleters_;

  Clock* clock_ = nullptr;
  TaskQueueFactory* task_queue_factory_ = nullptr;
  const FieldTrialsView* experiments_ = nullptr;
  RtcEventLog* event_log_ = nullptr;
};

}  // namespace webrtc

#endif  // API_CONNECTION_ENVIRONMENT_BUILDER_H_
