/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/connection_environment_builder.h"

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

Clock* ConnectionEnvironmentBuilder::DefaultClock() {
  return Clock::GetRealTimeClock();
}

TaskQueueFactory* ConnectionEnvironmentBuilder::DefaultTaskQueueFactory() {
  static TaskQueueFactory* tqf = CreateDefaultTaskQueueFactory().release();
  return tqf;
}

const FieldTrialsView* ConnectionEnvironmentBuilder::DefaultExperiments() {
  static FieldTrialsView* expriments = new FieldTrialBasedConfig;
  return expriments;
}

RtcEventLog* ConnectionEnvironmentBuilder::DefaultRtcEventLog() {
  static RtcEventLog* event_logger = new RtcEventLogNull;
  return event_logger;
}

}  // namespace webrtc
