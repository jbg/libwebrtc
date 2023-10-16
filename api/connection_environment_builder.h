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
  explicit ConnectionEnvironmentBuilder(const ConnectionEnvironment& env)
      : env_(env) {}

  ConnectionEnvironmentBuilder(const ConnectionEnvironmentBuilder&) = default;
  ConnectionEnvironmentBuilder& operator=(const ConnectionEnvironmentBuilder&) =
      default;

  ~ConnectionEnvironmentBuilder() = default;

  ConnectionEnvironment Build();

  // With functions set non-owning reference to the build Context, or do
  // nothing when passed pointer is nullptr.
  ConnectionEnvironmentBuilder& With(Clock* clock);
  ConnectionEnvironmentBuilder& With(TaskQueueFactory* task_queue_factory);
  ConnectionEnvironmentBuilder& With(const FieldTrialsView* experiments);
  ConnectionEnvironmentBuilder& With(RtcEventLog* event_logger);

 private:
  static Clock* DefaultClock();
  static TaskQueueFactory* DefaultTaskQueueFactory();
  static const FieldTrialsView* DefaultExperiments();
  static RtcEventLog* DefaultRtcEventLog();

  ConnectionEnvironment env_;
};

// Implementation details. Inlined so that optimizer can remove calls to
// `Default` functions when not needed.
inline ConnectionEnvironment ConnectionEnvironmentBuilder::Build() {
  if (env_.clock_ == nullptr) {
    env_.clock_ = DefaultClock();
  }
  if (env_.task_queue_factory_ == nullptr) {
    env_.task_queue_factory_ = DefaultTaskQueueFactory();
  }
  if (env_.experiments_ == nullptr) {
    env_.experiments_ = DefaultExperiments();
  }
  if (env_.event_log_ == nullptr) {
    env_.event_log_ = DefaultRtcEventLog();
  }
  return env_;
}

inline ConnectionEnvironmentBuilder& ConnectionEnvironmentBuilder::With(
    Clock* clock) {
  if (clock != nullptr) {
    env_.clock_ = clock;
  }
  return *this;
}

inline ConnectionEnvironmentBuilder& ConnectionEnvironmentBuilder::With(
    TaskQueueFactory* task_queue_factory) {
  if (task_queue_factory != nullptr) {
    env_.task_queue_factory_ = task_queue_factory;
  }
  return *this;
}

inline ConnectionEnvironmentBuilder& ConnectionEnvironmentBuilder::With(
    const FieldTrialsView* experiments) {
  if (experiments != nullptr) {
    env_.experiments_ = experiments;
  }
  return *this;
}

inline ConnectionEnvironmentBuilder& ConnectionEnvironmentBuilder::With(
    RtcEventLog* event_logger) {
  if (event_logger != nullptr) {
    env_.event_log_ = event_logger;
  }
  return *this;
}

}  // namespace webrtc

#endif  // API_CONNECTION_ENVIRONMENT_BUILDER_H_
