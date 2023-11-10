/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(danilchap): Write general documentation about purpose of the
// `Environment`

#ifndef API_ENVIRONMENT_ENVIRONMENT_H_
#define API_ENVIRONMENT_ENVIRONMENT_H_

#include <utility>

#include "absl/base/nullability.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// These classes are forward declared to keep Environment dependencies
// lightweight. User that needs to use any of the type below have to include
// their header explicitely.
class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

// Propagates common tools from webrtc api border down to individual components.
// Object of this class should be passed as a construction parameter and saved
// by value in each class that needs it. Most classes shouldn't create a fresh
// instance of the `Environment`, but instead use a copy.
// Example:
//    class WebrtcSession {
//     public:
//      WebrtcSession(const Environment& env, ...)
//          : env_(env),
//            rtcp_(env, ...),
//            ...
//
//      std::unique_ptr<VideoSenderStream> CreateVideoSender(...) {
//        return std::make_unique<VideoSenderStream>(env_, ...);
//      }
//
//     private:
//      const Environment env_;
//      RtcpTransceiver rtcp_;
//    }
// This class is thread safe.
class RTC_EXPORT Environment final {
 public:
  // Default constructor is deleted in favor of creating this object using
  // `EnvironmentFactory`. To create a default environment use
  // `CreateEnvironment()`.
  Environment() = delete;

  Environment(const Environment&) = default;
  Environment(Environment&&) = default;
  Environment& operator=(const Environment&) = default;
  Environment& operator=(Environment&&) = default;

  ~Environment() = default;

  const FieldTrialsView& field_trials() const;
  Clock& clock() const;
  TaskQueueFactory& task_queue_factory() const;
  RtcEventLog& event_log() const;

 private:
  friend class EnvironmentFactory;
  Environment(rtc::scoped_refptr<const rtc::RefCountedBase> storage,
              absl::Nonnull<const FieldTrialsView*> field_trials,
              absl::Nonnull<Clock*> clock,
              absl::Nonnull<TaskQueueFactory*> task_queue_factory,
              absl::Nonnull<RtcEventLog*> event_log)
      : storage_(std::move(storage)),
        field_trials_(field_trials),
        clock_(clock),
        task_queue_factory_(task_queue_factory),
        event_log_(event_log) {}

  // Container that keeps ownership of the dependencies below.
  // Pointers below assumed to be valid while object in the `storage_` is alive.
  rtc::scoped_refptr<const rtc::RefCountedBase> storage_;

  absl::Nonnull<const FieldTrialsView*> field_trials_;
  absl::Nonnull<Clock*> clock_;
  absl::Nonnull<TaskQueueFactory*> task_queue_factory_;
  absl::Nonnull<RtcEventLog*> event_log_;
};

// Implementation details below.
inline const FieldTrialsView& Environment::field_trials() const {
  return *field_trials_;
}
inline Clock& Environment::clock() const {
  return *clock_;
}
inline TaskQueueFactory& Environment::task_queue_factory() const {
  return *task_queue_factory_;
}
inline RtcEventLog& Environment::event_log() const {
  return *event_log_;
}

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_H_
