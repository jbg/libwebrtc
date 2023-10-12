/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CONTEXT_H_
#define API_CONTEXT_H_

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class ContextBuilder;
class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

class RTC_EXPORT Context {
 public:
  Context(const Context&) = default;
  Context& operator=(const Context&) = default;

  Clock& clock() const { return *clock_; }
  TaskQueueFactory& task_queue_factory() const { return *task_queue_factory_; }
  const FieldTrialsView& experiments() const { return *experiments_; }
  RtcEventLog& event_log() const { return *event_log_; }

 private:
  friend class ContextBuilder;
  // Creates Context in invalid state. It is up to the `ContextBuilder to ensure
  // Context state is valid when `ContextBuilder` returns it.
  Context() = default;

  Clock* clock_ = nullptr;
  TaskQueueFactory* task_queue_factory_ = nullptr;
  const FieldTrialsView* experiments_ = nullptr;
  RtcEventLog* event_log_ = nullptr;
};

}  // namespace webrtc

#endif  // API_CONTEXT_H_
