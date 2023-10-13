/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CONTEXT_BUILDER_H_
#define API_CONTEXT_BUILDER_H_

#include "api/context.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class ContextBuilder;
class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

class RTC_EXPORT ContextBuilder {
 public:
  ContextBuilder() = default;
  explicit ContextBuilder(const Context& context) : context_(context) {}

  ContextBuilder(const ContextBuilder&) = default;
  ContextBuilder& operator=(const ContextBuilder&) = default;

  ~ContextBuilder() = default;

  Context Build();

  // With functions set non-owning reference to the build Context, or do
  // nothing when passed pointer is nullptr.
  ContextBuilder& With(Clock* clock);
  ContextBuilder& With(TaskQueueFactory* task_queue_factory);
  ContextBuilder& With(const FieldTrialsView* experiments);
  ContextBuilder& With(RtcEventLog* event_logger);

 private:
  static Clock* DefaultClock();
  static TaskQueueFactory* DefaultTaskQueueFactory();
  static const FieldTrialsView* DefaultExperiments();
  static RtcEventLog* DefaultRtcEventLog();

  Context context_;
};

// Implementation details. Inlined so that optimizer can remove calls to
// `Default` functions when not needed.
inline Context ContextBuilder::Build() {
  if (context_.clock_ == nullptr) {
    context_.clock_ = DefaultClock();
  }
  if (context_.task_queue_factory_ == nullptr) {
    context_.task_queue_factory_ = DefaultTaskQueueFactory();
  }
  if (context_.experiments_ == nullptr) {
    context_.experiments_ = DefaultExperiments();
  }
  if (context_.event_log_ == nullptr) {
    context_.event_log_ = DefaultRtcEventLog();
  }
  return context_;
}

inline ContextBuilder& ContextBuilder::With(Clock* clock) {
  if (clock != nullptr) {
    context_.clock_ = clock;
  }
  return *this;
}

inline ContextBuilder& ContextBuilder::With(
    TaskQueueFactory* task_queue_factory) {
  if (task_queue_factory != nullptr) {
    context_.task_queue_factory_ = task_queue_factory;
  }
  return *this;
}

inline ContextBuilder& ContextBuilder::With(
    const FieldTrialsView* experiments) {
  if (experiments != nullptr) {
    context_.experiments_ = experiments;
  }
  return *this;
}

inline ContextBuilder& ContextBuilder::With(RtcEventLog* event_logger) {
  if (event_logger != nullptr) {
    context_.event_log_ = event_logger;
  }
  return *this;
}

}  // namespace webrtc

#endif  // API_CONTEXT_BUILDER_H_
