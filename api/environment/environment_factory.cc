/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/environment/environment_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

EnvironmentFactory::EnvironmentFactory(const Environment& env)
    : leaf_(env.storage_),
      clock_(env.clock_),
      task_queue_factory_(env.task_queue_factory_),
      field_trials_(env.field_trials_),
      event_log_(env.event_log_) {}

Environment EnvironmentFactory::Create() const {
  EnvironmentFactory b(*this);
  if (clock_ == nullptr) {
    b.With(Clock::GetRealTimeClock());
  }
  if (field_trials_ == nullptr) {
    b.With(std::make_unique<FieldTrialBasedConfig>());
  }
  if (task_queue_factory_ == nullptr) {
    b.With(CreateDefaultTaskQueueFactory(b.field_trials_));
  }
  if (event_log_ == nullptr) {
    b.With(std::make_unique<RtcEventLogNull>());
  }

  RTC_DCHECK(b.clock_ != nullptr);
  RTC_DCHECK(b.task_queue_factory_ != nullptr);
  RTC_DCHECK(b.field_trials_ != nullptr);
  RTC_DCHECK(b.event_log_ != nullptr);
  return Environment(std::move(b.leaf_),  //
                     b.field_trials_, b.clock_, b.task_queue_factory_,
                     b.event_log_);
}

}  // namespace webrtc
