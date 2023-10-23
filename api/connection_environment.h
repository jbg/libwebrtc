/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CONNECTION_ENVIRONMENT_H_
#define API_CONNECTION_ENVIRONMENT_H_

#include <utility>

#include "absl/base/nullability.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

class RTC_EXPORT ConnectionEnvironment {
 public:
  // Default constructor is deleted in favor of constructing this object using
  // `ConnectionEnvironmentBuilder`. To create a default environment
  // use `ConnectionEnvironmentBuilder().Build()`.
  friend class ConnectionEnvironmentBuilder;
  ConnectionEnvironment() = delete;

  ConnectionEnvironment(const ConnectionEnvironment&) = default;
  ConnectionEnvironment(ConnectionEnvironment&&) = default;
  ConnectionEnvironment& operator=(const ConnectionEnvironment&) = default;
  ConnectionEnvironment& operator=(ConnectionEnvironment&&) = default;

  const FieldTrialsView& experiments() const { return *experiments_; }
  Clock& clock() const { return *clock_; }
  TaskQueueFactory& task_queue_factory() const { return *task_queue_factory_; }
  RtcEventLog& event_log() const { return *event_log_; }

 private:
  // Classes that are allowed to create `ConnectionEnvironment`. This list
  // is very limited becase constructor below doesn't ensure the invariants of
  // the ConnectionEnvironment. It is the caller that must guarantee them.
  // Another reason to keep list small is extendability: when
  // ConnectionEnvironment would be extended, api above would be backward
  // compatible, but constructor below won't be.
  friend class PeerConnectionFactory;
  ConnectionEnvironment(rtc::scoped_refptr<const rtc::RefCountedBase> storage,
                        absl::Nonnull<const FieldTrialsView*> experiments,
                        absl::Nonnull<Clock*> clock,
                        absl::Nonnull<TaskQueueFactory*> task_queue_factory,
                        absl::Nonnull<RtcEventLog*> event_log)
      : storage_(std::move(storage)),
        experiments_(experiments),
        clock_(clock),
        task_queue_factory_(task_queue_factory),
        event_log_(event_log) {}

  // Container that keeps ownership of the dependencies below.
  // Pointers below assumed to be valid while object in the `storage_` is alive.
  rtc::scoped_refptr<const rtc::RefCountedBase> storage_;

  absl::Nonnull<const FieldTrialsView*> experiments_;
  absl::Nonnull<Clock*> clock_;
  absl::Nonnull<TaskQueueFactory*> task_queue_factory_;
  absl::Nonnull<RtcEventLog*> event_log_;
};

}  // namespace webrtc

#endif  // API_CONNECTION_ENVIRONMENT_H_
