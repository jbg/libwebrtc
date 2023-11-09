/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENVIRONMENT_ENVIRONMENT_BUILDER_H_
#define API_ENVIRONMENT_ENVIRONMENT_BUILDER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

// Constructs `Environment`.
// Tools passed to EnvironmentBuilder with ownership are saved in a shared
// storage inside Environment and thus would outlive all copies of the built
// Environment.
// Tools passed to EnvironmentBuilder without ownership should be valid while
// any copy of the Environment object created by the EnvironmentBuilder is
// alive. For tools that are not passed to the EnvironmentBuilder creates
// default implementation.
class RTC_EXPORT EnvironmentBuilder final {
 public:
  EnvironmentBuilder() = default;
  explicit EnvironmentBuilder(const Environment& env);

  EnvironmentBuilder(const EnvironmentBuilder&) = default;
  EnvironmentBuilder(EnvironmentBuilder&&) = default;
  EnvironmentBuilder& operator=(const EnvironmentBuilder&) = default;
  EnvironmentBuilder& operator=(EnvironmentBuilder&&) = default;

  ~EnvironmentBuilder() = default;

  Environment Build() const;

  // Adds a tool with ownership.
  // Does nothing when passed pointer is nullptr.
  template <typename T>
  EnvironmentBuilder& With(absl::Nullable<std::unique_ptr<T>> ptr);

  // Adds a tool without ownership.
  // Does nothing when passed pointer is nullptr.
  template <typename T>
  EnvironmentBuilder& With(absl::Nullable<T*> ptr);

 private:
  // Tree of values of any type for keeping ownership.
  template <typename T>
  class Item : public rtc::RefCountedBase {
   public:
    Item(rtc::scoped_refptr<const rtc::RefCountedBase> parent,
         std::unique_ptr<T> value)
        : parent_(std::move(parent)), value_(std::move(value)) {}
    ~Item() override = default;

   private:
    rtc::scoped_refptr<const rtc::RefCountedBase> parent_;
    std::unique_ptr<T> value_;
  };

  void Set(absl::Nonnull<Clock*> clock) { clock_ = clock; }
  void Set(absl::Nonnull<TaskQueueFactory*> task_queue_factory) {
    task_queue_factory_ = task_queue_factory;
  }
  void Set(absl::Nonnull<const FieldTrialsView*> field_trials) {
    field_trials_ = field_trials;
  }
  void Set(absl::Nonnull<RtcEventLog*> event_log) { event_log_ = event_log; }

  rtc::scoped_refptr<const rtc::RefCountedBase> leaf_;

  absl::Nullable<Clock*> clock_ = nullptr;
  absl::Nullable<TaskQueueFactory*> task_queue_factory_ = nullptr;
  absl::Nullable<const FieldTrialsView*> field_trials_ = nullptr;
  absl::Nullable<RtcEventLog*> event_log_ = nullptr;
};

template <typename T>
EnvironmentBuilder& EnvironmentBuilder::With(std::unique_ptr<T> ptr) {
  if (ptr != nullptr) {
    Set(ptr.get());
    leaf_ = rtc::make_ref_counted<Item<T>>(leaf_, std::move(ptr));
  }
  return *this;
}

template <typename T>
EnvironmentBuilder& EnvironmentBuilder::With(T* ptr) {
  if (ptr != nullptr) {
    Set(ptr);
  }
  return *this;
}

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_BUILDER_H_
