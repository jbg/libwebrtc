/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "call/default_rtc_context.h"

#include "api/task_queue/global_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"

namespace webrtc {
RtcContext GetDefaultRtcContext() {
  // The members are leaked intentionally since they require no actual cleanup
  // work and we don't want to incur the destruction overhead at program exit.
  RtcContext global_context{
      Clock::GetRealTimeClock(), new RtcEventLogNullImpl(),
      new FieldTrialBasedConfig(), &GlobalTaskQueueFactory(),
      new DefaultProcessThreadFactory()};
  return global_context;
}
}  // namespace webrtc
